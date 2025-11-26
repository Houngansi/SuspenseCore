// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Base/SuspenseBaseWidget.h"
#include "Interfaces/UI/ISuspenseHealthStaminaWidget.h"
#include "Interfaces/Core/ISuspenseAttributeProvider.h"
#include "SuspenseHealthStaminaWidget.generated.h"

// Forward declarations
class UProgressBar;
class UTextBlock;
class UAbilitySystemComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Health and Stamina display widget with material support
 * Handles progress bar animations and basic material parameter updates
 * All visual customization should be done through Material Instances
 */
UCLASS()
class UISYSTEM_API USuspenseHealthStaminaWidget : public USuspenseBaseWidget, public ISuspenseHealthStaminaWidgetInterface
{
    GENERATED_BODY()

public:
    USuspenseHealthStaminaWidget(const FObjectInitializer& ObjectInitializer);

    // Native widget lifecycle
    virtual void NativeConstruct() override;
    virtual void NativePreConstruct() override;
    
    // Base widget implementation
    virtual void InitializeWidget_Implementation() override;
    virtual void UninitializeWidget_Implementation() override;
    virtual void UpdateWidget_Implementation(float DeltaTime) override;

    // ISuspenseHealthStaminaWidgetInterface implementation
    virtual void InitializeWithASC_Implementation(UAbilitySystemComponent* ASC) override;
    virtual void UpdateHealth_Implementation(float CurrentHealth, float MaxHealth) override;
    virtual float GetHealthPercentage_Implementation() const override;
    virtual void SetAnimateHealthChanges_Implementation(bool bAnimate) override;
    virtual void UpdateStamina_Implementation(float CurrentStamina, float MaxStamina) override;
    virtual float GetStaminaPercentage_Implementation() const override;
    virtual void SetAnimateStaminaChanges_Implementation(bool bAnimate) override;
    virtual void SetInterpolationSpeeds_Implementation(float HealthSpeed, float StaminaSpeed, float BarSpeed) override;
    virtual void ForceImmediateUpdate_Implementation() override;

    // Attribute provider support
    UFUNCTION(BlueprintCallable, Category = "UI|Attributes")
    void InitializeWithProvider(TScriptInterface<ISuspenseAttributeProvider> Provider);

    UFUNCTION(BlueprintCallable, Category = "UI|Attributes")
    void ClearProvider();

    UFUNCTION(BlueprintPure, Category = "UI|Attributes")
    bool HasValidProvider() const;

    UFUNCTION(BlueprintCallable, Category = "UI|Attributes")
    void UpdateFromAttributeData(const FSuspenseAttributeData& HealthData, const FSuspenseAttributeData& StaminaData);

    // Value getters
    UFUNCTION(BlueprintPure, Category = "UI|Health")
    float GetCurrentHealth() const { return SmoothHealthValue; }

    UFUNCTION(BlueprintPure, Category = "UI|Health")
    float GetMaxHealth() const { return CachedMaxHealth; }

    UFUNCTION(BlueprintPure, Category = "UI|Stamina")
    float GetCurrentStamina() const { return SmoothStaminaValue; }

    UFUNCTION(BlueprintPure, Category = "UI|Stamina")
    float GetMaxStamina() const { return CachedMaxStamina; }

protected:
    // Widget bindings - must match blueprint widget names
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UProgressBar* HealthBar;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UProgressBar* StaminaBar;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* HealthText;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* StaminaText;
    
    // Display format for text values
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Format")
    FString ValueFormat = TEXT("{Current} / {Max}");
    
    // Animation settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Animation", meta = (DisplayName = "Animate Health Changes"))
    bool bAnimateHealth = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Animation", meta = (DisplayName = "Animate Stamina Changes"))
    bool bAnimateStamina = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Animation", meta = (ClampMin = 0.1, DisplayName = "Health Animation Speed"))
    float HealthInterpSpeed = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Animation", meta = (ClampMin = 0.1, DisplayName = "Stamina Animation Speed"))
    float StaminaInterpSpeed = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Animation", meta = (ClampMin = 0.1, DisplayName = "Bar Fill Animation Speed"))
    float BarInterpSpeed = 5.0f;

    // Material references - all visual customization should be done in Material Instances
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Materials", meta = (DisplayName = "Health Bar Material"))
    UMaterialInterface* HealthBarMaterial;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Materials", meta = (DisplayName = "Stamina Bar Material"))
    UMaterialInterface* StaminaBarMaterial;

    // Basic material parameter names that the material expects
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Materials|Parameters", meta = (DisplayName = "Fill Amount Parameter"))
    FName FillAmountParameterName = TEXT("FillAmount");
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Materials|Parameters", meta = (DisplayName = "Time Parameter"))
    FName TimeParameterName = TEXT("Time");

    // Background settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Background", meta = (DisplayName = "Use Custom Background"))
    bool bUseCustomBackground = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Background", meta = (DisplayName = "Health Bar Background Texture", EditCondition = "bUseCustomBackground"))
    UTexture2D* HealthBarBackgroundTexture;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Background", meta = (DisplayName = "Stamina Bar Background Texture", EditCondition = "bUseCustomBackground"))
    UTexture2D* StaminaBarBackgroundTexture;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Background", meta = (DisplayName = "Background Tint Color"))
    FLinearColor BackgroundTintColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Background", meta = (DisplayName = "Background Opacity", ClampMin = 0.0, ClampMax = 1.0))
    float BackgroundOpacity = 0.8f;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    // Initialization
    void InitializeMaterials();
    
    // Update functions
    void UpdateHealthUI();
    void UpdateStaminaUI();
    void UpdateHealthMaterialParameters();
    void UpdateStaminaMaterialParameters();
    void UpdateFromProvider();
    
    // Event handling
    void SubscribeToEvents();
    void UnsubscribeFromEvents();
    void OnHealthUpdated(float Current, float Max, float Percent);
    void OnStaminaUpdated(float Current, float Max, float Percent);

    // Smoothed values for animation
    float SmoothHealthValue = 100.0f;
    float SmoothStaminaValue = 100.0f;
    float SmoothHealthPercent = 1.0f;
    float SmoothStaminaPercent = 1.0f;
    
    // Target values to interpolate towards
    float TargetHealthValue = 100.0f;
    float TargetStaminaValue = 100.0f;
    
    // Cached maximum values
    float CachedMaxHealth = 100.0f;
    float CachedMaxStamina = 100.0f;
    
    // Time accumulator for material animations
    float MaterialTimeAccumulator = 0.0f;
    
    // Attribute provider reference
    UPROPERTY()
    TScriptInterface<ISuspenseAttributeProvider> AttributeProvider;
    
    // Event subscription handles
    FDelegateHandle HealthUpdateHandle;
    FDelegateHandle StaminaUpdateHandle;
    
    // Dynamic material instances created at runtime
    UPROPERTY()
    UMaterialInstanceDynamic* HealthBarDynamicMaterial;
    
    UPROPERTY()
    UMaterialInstanceDynamic* StaminaBarDynamicMaterial;
};