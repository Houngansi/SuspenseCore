// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Base/MedComBaseWidget.h"
#include "Interfaces/UI/IMedComWeaponUIWidgetInterface.h"
#include "GameplayTagContainer.h"
#include "MedComWeaponUIWidget.generated.h"

// Forward declarations
class UTextBlock;
class UImage;
class UTexture2D;
class UProgressBar;

UCLASS()
class MEDCOMUI_API UMedComWeaponUIWidget : public UMedComBaseWidget, public IMedComWeaponUIWidgetInterface
{
    GENERATED_BODY()
    
public:
    UMedComWeaponUIWidget(const FObjectInitializer& ObjectInitializer);

    virtual void InitializeWidget_Implementation() override;
    virtual void UninitializeWidget_Implementation() override;
    virtual void UpdateWidget_Implementation(float DeltaTime) override;

    // IMedComWeaponUIWidgetInterface implementation - обновленные сигнатуры
    virtual void SetWeapon_Implementation(AActor* Weapon) override;
    virtual void ClearWeapon_Implementation() override;
    virtual AActor* GetWeapon_Implementation() const override;
    virtual void UpdateAmmoDisplay_Implementation(float CurrentAmmo, float RemainingAmmo, float MagazineSize) override;
    virtual void SetAmmoDisplayFormat_Implementation(const FString& Format) override;
    virtual void UpdateFireMode_Implementation(const FGameplayTag& FireModeTag, const FText& DisplayName) override;
    virtual void SetAvailableFireModes_Implementation(const TArray<FGameplayTag>& AvailableModes) override;
    virtual void ShowReloadIndicator_Implementation(float ReloadTime, float ElapsedTime) override;
    virtual void HideReloadIndicator_Implementation() override;
    virtual void UpdateWeaponState_Implementation(const FGameplayTag& StateTag, bool bIsActive) override;

    UFUNCTION(BlueprintCallable, Category = "UI|Weapon")
    void RefreshWeaponDisplay();
    
    UFUNCTION(BlueprintPure, Category = "UI|Weapon")
    float GetAmmoPercentage() const;
    
    UFUNCTION(BlueprintPure, Category = "UI|Weapon")
    bool IsReloading() const { return bIsReloading; }

    UFUNCTION(BlueprintPure, Category = "UI|Weapon")
    AActor* GetWeaponActor() const { return CachedWeaponActor; }

protected:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* CurrentAmmoText;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* MaxAmmoText;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* RemainingAmmoText;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* WeaponNameText;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* FireModeText;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* WeaponIcon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UProgressBar* ReloadProgressBar;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI|Style")
    FLinearColor NormalAmmoColor = FLinearColor::White;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI|Style")
    FLinearColor LowAmmoColor = FLinearColor::Red;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI|Style")
    FLinearColor CriticalAmmoColor = FLinearColor(1.0f, 0.3f, 0.0f);
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI|Style", meta = (ClampMin = 0))
    int32 LowAmmoThreshold = 10;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI|Style", meta = (ClampMin = 0))
    int32 CriticalAmmoThreshold = 3;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI|Format")
    FString AmmoDisplayFormat = TEXT("{0} / {1}");
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI|FireModes")
    TMap<FString, FText> FireModeDisplayNames;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI|Style")
    FLinearColor ReloadIndicatorColor = FLinearColor::Yellow;

private:
    void SubscribeToEvents();
    void UnsubscribeFromEvents();
    void SetWeaponInternal(AActor* WeaponActor);
    void UpdateFromWeaponInterfaces();
    void SetWeaponIcon(UTexture2D* Icon);
    void UpdateAmmoTextStyle(float CurrentAmmo, float MaxAmmo);
    void ResetWeaponDisplay();
    void UpdateCurrentFireMode();
    bool ValidateWidgetBindings() const;

    void OnAmmoChanged(float CurrentAmmo, float RemainingAmmo, float MagazineSize);
    void OnWeaponStateChanged(FGameplayTag OldState, FGameplayTag NewState, bool bInterrupted);
    void OnWeaponReloadStart();
    void OnWeaponReloadEnd();
    void OnActiveWeaponChanged(AActor* NewWeapon);

    UPROPERTY()
    AActor* CachedWeaponActor = nullptr;
    
    FGameplayTag CurrentWeaponState;
    FGameplayTag CurrentFireMode;
    TArray<FGameplayTag> AvailableFireModes;
    
    bool bIsReloading = false;
    float CurrentReloadTime = 0.0f;
    float TotalReloadTime = 0.0f;
    
    float LastCurrentAmmo = 0.0f;
    float LastRemainingAmmo = 0.0f;
    float LastMagazineSize = 0.0f;
    
    float FireModeCheckInterval = 0.5f;
    float TimeSinceLastFireModeCheck = 0.0f;

    FDelegateHandle AmmoChangedHandle;
    FDelegateHandle WeaponStateChangedHandle;
    FDelegateHandle WeaponReloadStartHandle;
    FDelegateHandle WeaponReloadEndHandle;
    FDelegateHandle ActiveWeaponChangedHandle;
    FTimerHandle ReloadTimerHandle;
};