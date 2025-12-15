// Copyright Suspense Team Team. All Rights Reserved.

#include "Widgets/HUD/SuspenseHealthStaminaWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "SuspenseCore/Interfaces/Core/ISuspenseCoreAttributeProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "Math/UnrealMathUtility.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateTypes.h"

USuspenseHealthStaminaWidget::USuspenseHealthStaminaWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bEnableTick = true;
    WidgetTag = FGameplayTag::RequestGameplayTag(TEXT("UI.HUD.HealthStamina"));
}

void USuspenseHealthStaminaWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    // Initialize materials in designer preview
    InitializeMaterials();
}

void USuspenseHealthStaminaWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Initialize materials for runtime
    InitializeMaterials();
}

void USuspenseHealthStaminaWidget::InitializeWidget_Implementation()
{
    Super::InitializeWidget_Implementation();

    // Validate widget bindings
    if (!HealthBar || !HealthText || !StaminaBar || !StaminaText)
    {
        UE_LOG(LogTemp, Error, TEXT("[HealthStaminaWidget] Critical error: UI elements are not properly bound!"));
        UE_LOG(LogTemp, Error, TEXT("  - HealthBar: %s"), HealthBar ? TEXT("OK") : TEXT("MISSING"));
        UE_LOG(LogTemp, Error, TEXT("  - HealthText: %s"), HealthText ? TEXT("OK") : TEXT("MISSING"));
        UE_LOG(LogTemp, Error, TEXT("  - StaminaBar: %s"), StaminaBar ? TEXT("OK") : TEXT("MISSING"));
        UE_LOG(LogTemp, Error, TEXT("  - StaminaText: %s"), StaminaText ? TEXT("OK") : TEXT("MISSING"));
    }

    // Reset time accumulator
    MaterialTimeAccumulator = 0.0f;

    // Initialize materials
    InitializeMaterials();

    // Subscribe to events
    SubscribeToEvents();

    // Initial UI update
    UpdateHealthUI();
    UpdateStaminaUI();

    UE_LOG(LogTemp, Log, TEXT("[HealthStaminaWidget] Widget initialized successfully"));
}

void USuspenseHealthStaminaWidget::UninitializeWidget_Implementation()
{
    // Clean up
    ClearProvider();
    UnsubscribeFromEvents();

    // Clean up dynamic materials
    HealthBarDynamicMaterial = nullptr;
    StaminaBarDynamicMaterial = nullptr;

    Super::UninitializeWidget_Implementation();

    UE_LOG(LogTemp, Log, TEXT("[HealthStaminaWidget] Widget uninitialized"));
}

void USuspenseHealthStaminaWidget::InitializeMaterials()
{
    // Initialize health bar
    if (HealthBar)
    {
        FProgressBarStyle UpdatedStyle = HealthBar->GetWidgetStyle();

        // Configure background - NO color changes! Colors from material in Editor
        if (bUseCustomBackground && HealthBarBackgroundTexture)
        {
            UpdatedStyle.BackgroundImage.SetResourceObject(HealthBarBackgroundTexture);
            UpdatedStyle.BackgroundImage.DrawAs = ESlateBrushDrawType::Box;
            UpdatedStyle.BackgroundImage.Tiling = ESlateBrushTileType::NoTile;
            UpdatedStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor::White); // No tint - use material colors
            UpdatedStyle.BackgroundImage.Margin = FMargin(0.0f);
        }
        // If no custom background, don't override - let Editor material handle it

        // Configure fill material - NO color changes! Colors from material in Editor
        if (HealthBarMaterial)
        {
            HealthBarDynamicMaterial = UMaterialInstanceDynamic::Create(HealthBarMaterial, this);
            if (HealthBarDynamicMaterial)
            {
                UpdatedStyle.FillImage.SetResourceObject(HealthBarDynamicMaterial);
                UpdatedStyle.FillImage.DrawAs = ESlateBrushDrawType::Box;
                UpdatedStyle.FillImage.Tiling = ESlateBrushTileType::NoTile;
                UpdatedStyle.FillImage.TintColor = FSlateColor(FLinearColor::White); // No tint - use material colors

                // Set initial parameters
                UpdateHealthMaterialParameters();
            }
        }

        HealthBar->SetWidgetStyle(UpdatedStyle);
    }

    // Initialize stamina bar
    if (StaminaBar)
    {
        FProgressBarStyle UpdatedStyle = StaminaBar->GetWidgetStyle();

        // Configure background - NO color changes! Colors from material in Editor
        if (bUseCustomBackground && StaminaBarBackgroundTexture)
        {
            UpdatedStyle.BackgroundImage.SetResourceObject(StaminaBarBackgroundTexture);
            UpdatedStyle.BackgroundImage.DrawAs = ESlateBrushDrawType::Box;
            UpdatedStyle.BackgroundImage.Tiling = ESlateBrushTileType::NoTile;
            UpdatedStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor::White); // No tint - use material colors
            UpdatedStyle.BackgroundImage.Margin = FMargin(0.0f);
        }
        // If no custom background, don't override - let Editor material handle it

        // Configure fill material - NO color changes! Colors from material in Editor
        if (StaminaBarMaterial)
        {
            StaminaBarDynamicMaterial = UMaterialInstanceDynamic::Create(StaminaBarMaterial, this);
            if (StaminaBarDynamicMaterial)
            {
                UpdatedStyle.FillImage.SetResourceObject(StaminaBarDynamicMaterial);
                UpdatedStyle.FillImage.DrawAs = ESlateBrushDrawType::Box;
                UpdatedStyle.FillImage.Tiling = ESlateBrushTileType::NoTile;
                UpdatedStyle.FillImage.TintColor = FSlateColor(FLinearColor::White); // No tint - use material colors

                // Set initial parameters
                UpdateStaminaMaterialParameters();
            }
        }

        StaminaBar->SetWidgetStyle(UpdatedStyle);
    }
}

void USuspenseHealthStaminaWidget::UpdateWidget_Implementation(float DeltaTime)
{
    Super::UpdateWidget_Implementation(DeltaTime);

    // Update time for material animations
    MaterialTimeAccumulator += DeltaTime;

    // Update from provider if available
    if (HasValidProvider())
    {
        UpdateFromProvider();
    }

    // Health interpolation
    if (bAnimateHealth)
    {
        SmoothHealthValue = FMath::FInterpTo(SmoothHealthValue, TargetHealthValue, DeltaTime, HealthInterpSpeed);
        float TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (TargetHealthValue / CachedMaxHealth) : 0.0f;
        SmoothHealthPercent = FMath::FInterpTo(SmoothHealthPercent, TargetHealthPercent, DeltaTime, BarInterpSpeed);
    }
    else
    {
        SmoothHealthValue = TargetHealthValue;
        SmoothHealthPercent = (CachedMaxHealth > 0.0f) ? (TargetHealthValue / CachedMaxHealth) : 0.0f;
    }

    // Stamina interpolation
    if (bAnimateStamina)
    {
        SmoothStaminaValue = FMath::FInterpTo(SmoothStaminaValue, TargetStaminaValue, DeltaTime, StaminaInterpSpeed);
        float TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (TargetStaminaValue / CachedMaxStamina) : 0.0f;
        SmoothStaminaPercent = FMath::FInterpTo(SmoothStaminaPercent, TargetStaminaPercent, DeltaTime, BarInterpSpeed);
    }
    else
    {
        SmoothStaminaValue = TargetStaminaValue;
        SmoothStaminaPercent = (CachedMaxStamina > 0.0f) ? (TargetStaminaValue / CachedMaxStamina) : 0.0f;
    }

    // Update UI
    UpdateHealthUI();
    UpdateStaminaUI();
}

void USuspenseHealthStaminaWidget::InitializeWithASC_Implementation(UAbilitySystemComponent* ASC)
{
    UE_LOG(LogTemp, Warning, TEXT("[HealthStaminaWidget] InitializeWithASC is deprecated - use InitializeWithProvider instead"));
}

void USuspenseHealthStaminaWidget::InitializeWithProvider(TScriptInterface<ISuspenseCoreAttributeProvider> Provider)
{
    AttributeProvider = Provider;

    UE_LOG(LogTemp, Log, TEXT("[HealthStaminaWidget] Initializing with attribute provider"));

    if (HasValidProvider())
    {
        FSuspenseCoreAttributeData HealthData = ISuspenseCoreAttributeProvider::Execute_GetHealthData(AttributeProvider.GetObject());
        FSuspenseCoreAttributeData StaminaData = ISuspenseCoreAttributeProvider::Execute_GetStaminaData(AttributeProvider.GetObject());

        UE_LOG(LogTemp, Log, TEXT("[HealthStaminaWidget] Provider data received:"));
        UE_LOG(LogTemp, Log, TEXT("  - Health: Current=%.1f, Max=%.1f, Valid=%s"),
            HealthData.CurrentValue, HealthData.MaxValue, HealthData.bIsValid ? TEXT("true") : TEXT("false"));
        UE_LOG(LogTemp, Log, TEXT("  - Stamina: Current=%.1f, Max=%.1f, Valid=%s"),
            StaminaData.CurrentValue, StaminaData.MaxValue, StaminaData.bIsValid ? TEXT("true") : TEXT("false"));

        UpdateFromAttributeData(HealthData, StaminaData);
        ForceImmediateUpdate_Implementation();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[HealthStaminaWidget] Provider is not valid!"));
    }
}

void USuspenseHealthStaminaWidget::ClearProvider()
{
    AttributeProvider.SetInterface(nullptr);
    AttributeProvider.SetObject(nullptr);
}

bool USuspenseHealthStaminaWidget::HasValidProvider() const
{
    return AttributeProvider.GetInterface() != nullptr;
}

void USuspenseHealthStaminaWidget::UpdateFromAttributeData(const FSuspenseCoreAttributeData& HealthData, const FSuspenseCoreAttributeData& StaminaData)
{
    if (HealthData.bIsValid)
    {
        TargetHealthValue = HealthData.CurrentValue;
        CachedMaxHealth = HealthData.MaxValue;
    }

    if (StaminaData.bIsValid)
    {
        TargetStaminaValue = StaminaData.CurrentValue;
        CachedMaxStamina = StaminaData.MaxValue;
    }
}

void USuspenseHealthStaminaWidget::UpdateHealth_Implementation(float CurrentHealth, float MaxHealth)
{
    TargetHealthValue = CurrentHealth;
    CachedMaxHealth = MaxHealth;

    if (!bAnimateHealth)
    {
        SmoothHealthValue = CurrentHealth;
        SmoothHealthPercent = (MaxHealth > 0.0f) ? (CurrentHealth / MaxHealth) : 0.0f;
        UpdateHealthUI();
    }
}

float USuspenseHealthStaminaWidget::GetHealthPercentage_Implementation() const
{
    return SmoothHealthPercent;
}

void USuspenseHealthStaminaWidget::SetAnimateHealthChanges_Implementation(bool bAnimate)
{
    bAnimateHealth = bAnimate;
}

void USuspenseHealthStaminaWidget::UpdateStamina_Implementation(float CurrentStamina, float MaxStamina)
{
    TargetStaminaValue = CurrentStamina;
    CachedMaxStamina = MaxStamina;

    if (!bAnimateStamina)
    {
        SmoothStaminaValue = CurrentStamina;
        SmoothStaminaPercent = (MaxStamina > 0.0f) ? (CurrentStamina / MaxStamina) : 0.0f;
        UpdateStaminaUI();
    }
}

float USuspenseHealthStaminaWidget::GetStaminaPercentage_Implementation() const
{
    return SmoothStaminaPercent;
}

void USuspenseHealthStaminaWidget::SetAnimateStaminaChanges_Implementation(bool bAnimate)
{
    bAnimateStamina = bAnimate;
}

void USuspenseHealthStaminaWidget::SetInterpolationSpeeds_Implementation(float HealthSpeed, float StaminaSpeed, float BarSpeed)
{
    HealthInterpSpeed = FMath::Max(0.1f, HealthSpeed);
    StaminaInterpSpeed = FMath::Max(0.1f, StaminaSpeed);
    BarInterpSpeed = FMath::Max(0.1f, BarSpeed);

    UE_LOG(LogTemp, Log, TEXT("[HealthStaminaWidget] Interpolation speeds updated - Health: %.1f, Stamina: %.1f, Bar: %.1f"),
        HealthInterpSpeed, StaminaInterpSpeed, BarInterpSpeed);
}

void USuspenseHealthStaminaWidget::ForceImmediateUpdate_Implementation()
{
    // Ensure valid max values
    if (CachedMaxHealth <= 0.0f)
    {
        CachedMaxHealth = 100.0f;
        UE_LOG(LogTemp, Warning, TEXT("[HealthStaminaWidget] MaxHealth was 0, defaulting to 100"));
    }

    if (CachedMaxStamina <= 0.0f)
    {
        CachedMaxStamina = 100.0f;
        UE_LOG(LogTemp, Warning, TEXT("[HealthStaminaWidget] MaxStamina was 0, defaulting to 100"));
    }

    // Force immediate updates
    SmoothHealthValue = TargetHealthValue;
    SmoothHealthPercent = TargetHealthValue / CachedMaxHealth;
    SmoothStaminaValue = TargetStaminaValue;
    SmoothStaminaPercent = TargetStaminaValue / CachedMaxStamina;

    UE_LOG(LogTemp, Log, TEXT("[HealthStaminaWidget] Forced immediate update - Health: %.1f/%.1f (%.1f%%), Stamina: %.1f/%.1f (%.1f%%)"),
        SmoothHealthValue, CachedMaxHealth, SmoothHealthPercent * 100.0f,
        SmoothStaminaValue, CachedMaxStamina, SmoothStaminaPercent * 100.0f);

    UpdateHealthUI();
    UpdateStaminaUI();
}

void USuspenseHealthStaminaWidget::UpdateHealthUI()
{
    if (HealthBar)
    {
        HealthBar->SetPercent(SmoothHealthPercent);
        UpdateHealthMaterialParameters();
    }

    if (HealthText)
    {
        FFormatNamedArguments Args;
        Args.Add(TEXT("Current"), FText::AsNumber(FMath::RoundToInt(SmoothHealthValue)));
        Args.Add(TEXT("Max"), FText::AsNumber(FMath::RoundToInt(CachedMaxHealth)));

        FText FormatPattern = FText::FromString(ValueFormat);
        FText HealthString = FText::Format(FormatPattern, Args);

        HealthText->SetText(HealthString);
    }
}

void USuspenseHealthStaminaWidget::UpdateStaminaUI()
{
    if (StaminaBar)
    {
        StaminaBar->SetPercent(SmoothStaminaPercent);
        UpdateStaminaMaterialParameters();
    }

    if (StaminaText)
    {
        FFormatNamedArguments Args;
        Args.Add(TEXT("Current"), FText::AsNumber(FMath::RoundToInt(SmoothStaminaValue)));
        Args.Add(TEXT("Max"), FText::AsNumber(FMath::RoundToInt(CachedMaxStamina)));

        FText FormatPattern = FText::FromString(ValueFormat);
        FText StaminaString = FText::Format(FormatPattern, Args);

        StaminaText->SetText(StaminaString);
    }
}

void USuspenseHealthStaminaWidget::UpdateHealthMaterialParameters()
{
    if (!HealthBarDynamicMaterial)
        return;

    // Only update essential parameters - FillAmount and Time
    // All visual customization should be done in the Material Instance
    HealthBarDynamicMaterial->SetScalarParameterValue(FillAmountParameterName, SmoothHealthPercent);
    HealthBarDynamicMaterial->SetScalarParameterValue(TimeParameterName, MaterialTimeAccumulator);

    UE_LOG(LogTemp, VeryVerbose, TEXT("[HealthStaminaWidget] Updated health material - Fill: %.2f, Time: %.2f"),
        SmoothHealthPercent, MaterialTimeAccumulator);
}

void USuspenseHealthStaminaWidget::UpdateStaminaMaterialParameters()
{
    if (!StaminaBarDynamicMaterial)
        return;

    // Only update essential parameters - FillAmount and Time
    // All visual customization should be done in the Material Instance
    StaminaBarDynamicMaterial->SetScalarParameterValue(FillAmountParameterName, SmoothStaminaPercent);
    StaminaBarDynamicMaterial->SetScalarParameterValue(TimeParameterName, MaterialTimeAccumulator);

    UE_LOG(LogTemp, VeryVerbose, TEXT("[HealthStaminaWidget] Updated stamina material - Fill: %.2f, Time: %.2f"),
        SmoothStaminaPercent, MaterialTimeAccumulator);
}

void USuspenseHealthStaminaWidget::UpdateFromProvider()
{
    if (!HasValidProvider())
        return;

    FSuspenseCoreAttributeData HealthData = ISuspenseCoreAttributeProvider::Execute_GetHealthData(AttributeProvider.GetObject());
    FSuspenseCoreAttributeData StaminaData = ISuspenseCoreAttributeProvider::Execute_GetStaminaData(AttributeProvider.GetObject());

    UpdateFromAttributeData(HealthData, StaminaData);
}

void USuspenseHealthStaminaWidget::SubscribeToEvents()
{
    USuspenseCoreEventManager* EventManager = GetDelegateManager();
    if (!EventManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[HealthStaminaWidget] EventManager not found"));
        return;
    }

    CachedEventBus = EventManager->GetEventBus();
    if (!CachedEventBus.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[HealthStaminaWidget] EventBus not found"));
        return;
    }

    // Subscribe to Health attribute events via EventBus
    HealthUpdateHandle = CachedEventBus->SubscribeNative(
        FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Health")),
        const_cast<USuspenseHealthStaminaWidget*>(this),
        FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseHealthStaminaWidget::HandleHealthEvent),
        ESuspenseCoreEventPriority::Normal
    );

    // Subscribe to Stamina attribute events via EventBus
    StaminaUpdateHandle = CachedEventBus->SubscribeNative(
        FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Stamina")),
        const_cast<USuspenseHealthStaminaWidget*>(this),
        FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseHealthStaminaWidget::HandleStaminaEvent),
        ESuspenseCoreEventPriority::Normal
    );

    UE_LOG(LogTemp, Log, TEXT("[HealthStaminaWidget] EventBus subscriptions complete"));
}

void USuspenseHealthStaminaWidget::UnsubscribeFromEvents()
{
    if (!CachedEventBus.IsValid())
    {
        return;
    }

    if (HealthUpdateHandle.IsValid())
    {
        CachedEventBus->Unsubscribe(HealthUpdateHandle);
    }

    if (StaminaUpdateHandle.IsValid())
    {
        CachedEventBus->Unsubscribe(StaminaUpdateHandle);
    }

    UE_LOG(LogTemp, Log, TEXT("[HealthStaminaWidget] Unsubscribed from events"));
}

void USuspenseHealthStaminaWidget::HandleHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    float Current = EventData.GetFloat(FName("Value"), 100.0f);
    float Max = EventData.GetFloat(FName("MaxValue"), 100.0f);
    float Percent = (Max > 0.0f) ? (Current / Max) : 1.0f;
    OnHealthUpdated(Current, Max, Percent);
}

void USuspenseHealthStaminaWidget::HandleStaminaEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    float Current = EventData.GetFloat(FName("Value"), 100.0f);
    float Max = EventData.GetFloat(FName("MaxValue"), 100.0f);
    float Percent = (Max > 0.0f) ? (Current / Max) : 1.0f;
    OnStaminaUpdated(Current, Max, Percent);
}

void USuspenseHealthStaminaWidget::OnHealthUpdated(float Current, float Max, float Percent)
{
    UpdateHealth_Implementation(Current, Max);
}

void USuspenseHealthStaminaWidget::OnStaminaUpdated(float Current, float Max, float Percent)
{
    UpdateStamina_Implementation(Current, Max);
}

#if WITH_EDITOR
void USuspenseHealthStaminaWidget::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (!PropertyChangedEvent.Property)
    {
        return;
    }

    const FName PropertyName = PropertyChangedEvent.Property->GetFName();

    // Reinitialize materials if material references changed
    if (PropertyName == GET_MEMBER_NAME_CHECKED(USuspenseHealthStaminaWidget, HealthBarMaterial) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(USuspenseHealthStaminaWidget, StaminaBarMaterial) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(USuspenseHealthStaminaWidget, HealthBarBackgroundTexture) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(USuspenseHealthStaminaWidget, StaminaBarBackgroundTexture))
    {
        UE_LOG(LogTemp, Log, TEXT("[HealthStaminaWidget] Material or background settings changed in editor"));

        InitializeMaterials();

        if (GetOuter())
        {
            GetOuter()->MarkPackageDirty();
        }
    }
}
#endif
