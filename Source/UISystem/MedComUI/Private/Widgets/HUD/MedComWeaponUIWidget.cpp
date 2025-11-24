// Copyright MedCom Team. All Rights Reserved.

#include "Widgets/HUD/MedComWeaponUIWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Delegates/EventDelegateManager.h"
#include "Interfaces/Weapon/IMedComWeaponInterface.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Math/UnrealMathUtility.h"

UMedComWeaponUIWidget::UMedComWeaponUIWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bEnableTick = true;
    WidgetTag = FGameplayTag::RequestGameplayTag(TEXT("UI.HUD.WeaponInfo"));
    
    // Initialize fire mode display names
    FireModeDisplayNames.Add(TEXT("Weapon.FireMode.Single"), FText::FromString(TEXT("SINGLE")));
    FireModeDisplayNames.Add(TEXT("Weapon.FireMode.Burst"), FText::FromString(TEXT("BURST")));
    FireModeDisplayNames.Add(TEXT("Weapon.FireMode.Auto"), FText::FromString(TEXT("AUTO")));
    
    // Initialize colors
    NormalAmmoColor = FLinearColor::White;
    LowAmmoColor = FLinearColor::Red;
    CriticalAmmoColor = FLinearColor(1.0f, 0.3f, 0.0f);
    ReloadIndicatorColor = FLinearColor::Yellow;
    
    // Initialize thresholds
    LowAmmoThreshold = 10;
    CriticalAmmoThreshold = 3;
    AmmoDisplayFormat = TEXT("{0} / {1}");
    FireModeCheckInterval = 0.5f;
    TimeSinceLastFireModeCheck = 0.0f;
}

void UMedComWeaponUIWidget::InitializeWidget_Implementation()
{
    Super::InitializeWidget_Implementation();
    
    if (!ValidateWidgetBindings())
    {
        UE_LOG(LogTemp, Error, TEXT("[MedComWeaponUIWidget] Failed to validate widget bindings"));
        return;
    }
    
    // Reset display to initial state
    ResetWeaponDisplay();
    
    // Subscribe to weapon events
    SubscribeToEvents();
    
    UE_LOG(LogTemp, Log, TEXT("[MedComWeaponUIWidget] Widget initialized"));
}

void UMedComWeaponUIWidget::UninitializeWidget_Implementation()
{
    // Clear any active timers
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReloadTimerHandle);
    }
    
    // Clear weapon reference
    ClearWeapon_Implementation();
    
    // Unsubscribe from events
    UnsubscribeFromEvents();
    
    Super::UninitializeWidget_Implementation();
    
    UE_LOG(LogTemp, Log, TEXT("[MedComWeaponUIWidget] Widget uninitialized"));
}

void UMedComWeaponUIWidget::UpdateWidget_Implementation(float DeltaTime)
{
    Super::UpdateWidget_Implementation(DeltaTime);
    
    // Update weapon data if we have a cached weapon
    if (CachedWeaponActor)
    {
        UpdateFromWeaponInterfaces();
        
        // Periodically check fire mode
        TimeSinceLastFireModeCheck += DeltaTime;
        if (TimeSinceLastFireModeCheck >= FireModeCheckInterval)
        {
            UpdateCurrentFireMode();
            TimeSinceLastFireModeCheck = 0.0f;
        }
    }
    
    // Update reload progress if reloading
    if (bIsReloading && ReloadProgressBar && TotalReloadTime > 0.0f)
    {
        CurrentReloadTime += DeltaTime;
        float Progress = FMath::Clamp(CurrentReloadTime / TotalReloadTime, 0.0f, 1.0f);
        ReloadProgressBar->SetPercent(Progress);
    }
}

void UMedComWeaponUIWidget::SetWeapon_Implementation(AActor* Weapon)
{
    SetWeaponInternal(Weapon);
}

void UMedComWeaponUIWidget::SetWeaponInternal(AActor* WeaponActor)
{
    // Early exit if same weapon
    if (WeaponActor == CachedWeaponActor)
        return;
        
    // Clear previous weapon
    ClearWeapon_Implementation();
    
    // Set new weapon
    CachedWeaponActor = WeaponActor;
    
    if (CachedWeaponActor)
    {
        UE_LOG(LogTemp, Log, TEXT("[MedComWeaponUIWidget] Setting weapon: %s"), *CachedWeaponActor->GetName());
        
        // Show all weapon UI elements
        if (WeaponNameText) WeaponNameText->SetVisibility(ESlateVisibility::Visible);
        if (CurrentAmmoText) CurrentAmmoText->SetVisibility(ESlateVisibility::Visible);
        if (MaxAmmoText) MaxAmmoText->SetVisibility(ESlateVisibility::Visible);
        if (RemainingAmmoText) RemainingAmmoText->SetVisibility(ESlateVisibility::Visible);
        if (FireModeText) FireModeText->SetVisibility(ESlateVisibility::Visible);
        if (WeaponIcon) WeaponIcon->SetVisibility(ESlateVisibility::Visible);
        
        // Refresh display with current weapon data
        RefreshWeaponDisplay();
        
        // REMOVED: BroadcastAmmoUpdated call - this was causing infinite recursion
        // The widget should only display data, not broadcast events
    }
    else
    {
        ResetWeaponDisplay();
    }
}

void UMedComWeaponUIWidget::ClearWeapon_Implementation()
{
    if (CachedWeaponActor)
    {
        UE_LOG(LogTemp, Log, TEXT("[MedComWeaponUIWidget] Clearing weapon: %s"), *CachedWeaponActor->GetName());
    }
    
    // Clear weapon reference
    CachedWeaponActor = nullptr;
    bIsReloading = false;
    
    // Reset state
    CurrentWeaponState = FGameplayTag();
    CurrentFireMode = FGameplayTag();
    AvailableFireModes.Empty();
    
    // Reset cached values
    LastCurrentAmmo = 0.0f;
    LastRemainingAmmo = 0.0f;
    LastMagazineSize = 0.0f;
    
    // Reset reload state
    CurrentReloadTime = 0.0f;
    TotalReloadTime = 0.0f;
    
    // Clear any active timers
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReloadTimerHandle);
    }
}

AActor* UMedComWeaponUIWidget::GetWeapon_Implementation() const
{
    return CachedWeaponActor;
}

void UMedComWeaponUIWidget::UpdateAmmoDisplay_Implementation(float CurrentAmmo, float RemainingAmmo, float MagazineSize)
{
    // Cache the values for later use
    LastCurrentAmmo = CurrentAmmo;
    LastRemainingAmmo = RemainingAmmo;
    LastMagazineSize = MagazineSize;
    
    // Update current ammo text
    if (CurrentAmmoText)
    {
        CurrentAmmoText->SetText(FText::AsNumber(FMath::FloorToInt(CurrentAmmo)));
    }
    
    // Update max ammo text
    if (MaxAmmoText)
    {
        MaxAmmoText->SetText(FText::AsNumber(FMath::FloorToInt(MagazineSize)));
    }
    
    // Update remaining ammo text
    if (RemainingAmmoText)
    {
        RemainingAmmoText->SetText(FText::AsNumber(FMath::FloorToInt(RemainingAmmo)));
    }
    
    // Update text style based on ammo amount
    UpdateAmmoTextStyle(CurrentAmmo, MagazineSize);
    
    // CRITICAL FIX: Removed BroadcastAmmoUpdated call here
    // This was causing infinite recursion as the widget would broadcast an event
    // that it was also listening to, creating an endless loop
    // UI widgets should only display data, not generate events
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("[MedComWeaponUIWidget] Ammo display updated: %.0f/%.0f (%.0f remaining)"), 
        CurrentAmmo, MagazineSize, RemainingAmmo);
}

void UMedComWeaponUIWidget::SetAmmoDisplayFormat_Implementation(const FString& Format)
{
    AmmoDisplayFormat = Format;
    
    // Refresh display with new format
    UpdateAmmoDisplay_Implementation(LastCurrentAmmo, LastRemainingAmmo, LastMagazineSize);
    
    UE_LOG(LogTemp, Log, TEXT("[MedComWeaponUIWidget] Ammo display format set to: %s"), *Format);
}

void UMedComWeaponUIWidget::UpdateFireMode_Implementation(const FGameplayTag& FireModeTag, const FText& DisplayName)
{
    CurrentFireMode = FireModeTag;
    
    if (FireModeText)
    {
        FText ModeText = DisplayName;
        
        // If no display name provided, try to determine it
        if (DisplayName.IsEmpty())
        {
            FString FireModeString = FireModeTag.ToString();
            
            // Check if we have a mapped display name
            if (FText* MappedName = FireModeDisplayNames.Find(FireModeString))
            {
                ModeText = *MappedName;
            }
            else
            {
                // Extract the last part of the tag as display name
                FString DisplayMode;
                if (FireModeString.Split(TEXT("."), nullptr, &DisplayMode, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
                {
                    ModeText = FText::FromString(DisplayMode.ToUpper());
                }
                else
                {
                    ModeText = FText::FromString(FireModeString);
                }
            }
        }
        
        FireModeText->SetText(ModeText);
    }
    
    UE_LOG(LogTemp, Verbose, TEXT("[MedComWeaponUIWidget] Fire mode updated: %s"), *FireModeTag.ToString());
}

void UMedComWeaponUIWidget::SetAvailableFireModes_Implementation(const TArray<FGameplayTag>& AvailableModes)
{
    AvailableFireModes = AvailableModes;
    UE_LOG(LogTemp, Log, TEXT("[MedComWeaponUIWidget] Available fire modes set: %d modes"), AvailableModes.Num());
}

void UMedComWeaponUIWidget::ShowReloadIndicator_Implementation(float ReloadTime, float ElapsedTime)
{
    bIsReloading = true;
    TotalReloadTime = ReloadTime;
    CurrentReloadTime = ElapsedTime;
    
    if (ReloadProgressBar)
    {
        ReloadProgressBar->SetVisibility(ESlateVisibility::Visible);
        ReloadProgressBar->SetFillColorAndOpacity(ReloadIndicatorColor);
        
        // Calculate and set initial progress
        float Progress = (TotalReloadTime > 0.0f) ? FMath::Clamp(CurrentReloadTime / TotalReloadTime, 0.0f, 1.0f) : 0.0f;
        ReloadProgressBar->SetPercent(Progress);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[MedComWeaponUIWidget] Reload indicator shown - Time: %.2f, Elapsed: %.2f"), 
        ReloadTime, ElapsedTime);
}

void UMedComWeaponUIWidget::HideReloadIndicator_Implementation()
{
    bIsReloading = false;
    CurrentReloadTime = 0.0f;
    TotalReloadTime = 0.0f;
    
    if (ReloadProgressBar)
    {
        ReloadProgressBar->SetVisibility(ESlateVisibility::Hidden);
    }
    
    // Clear any reload timers
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ReloadTimerHandle);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[MedComWeaponUIWidget] Reload indicator hidden"));
}

void UMedComWeaponUIWidget::UpdateWeaponState_Implementation(const FGameplayTag& StateTag, bool bIsActive)
{
    if (bIsActive)
    {
        CurrentWeaponState = StateTag;
        
        // Handle specific state transitions
        if (StateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Reloading"))))
        {
            // Reloading state is handled by ShowReloadIndicator
        }
        else if (CurrentWeaponState.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Reloading"))))
        {
            // If we were reloading but now in a different state, hide reload indicator
            HideReloadIndicator_Implementation();
        }
    }
    
    UE_LOG(LogTemp, Verbose, TEXT("[MedComWeaponUIWidget] Weapon state updated: %s (Active: %s)"), 
        *StateTag.ToString(), bIsActive ? TEXT("Yes") : TEXT("No"));
}

void UMedComWeaponUIWidget::RefreshWeaponDisplay()
{
    if (!CachedWeaponActor)
    {
        ResetWeaponDisplay();
        return;
    }
    
    // Update weapon name
    if (WeaponNameText)
    {
        FText WeaponName = FText::FromString(CachedWeaponActor->GetName());
        WeaponNameText->SetText(WeaponName);
    }
    
    // Update fire mode
    UpdateCurrentFireMode();
    
    // Update ammo and other data from weapon interfaces
    UpdateFromWeaponInterfaces();
    
    UE_LOG(LogTemp, Log, TEXT("[MedComWeaponUIWidget] Weapon display refreshed"));
}

float UMedComWeaponUIWidget::GetAmmoPercentage() const
{
    if (LastMagazineSize > 0.0f)
    {
        return LastCurrentAmmo / LastMagazineSize;
    }
    return 1.0f;
}

void UMedComWeaponUIWidget::SubscribeToEvents()
{
    if (UEventDelegateManager* EventManager = UMedComBaseWidget::GetDelegateManager())
    {
        // Subscribe to ammo changed events
        AmmoChangedHandle = EventManager->SubscribeToAmmoChanged(
            [this](float CurrentAmmo, float RemainingAmmo, float MagazineSize)
            {
                OnAmmoChanged(CurrentAmmo, RemainingAmmo, MagazineSize);
            });
            
        // Subscribe to weapon state changed events
        WeaponStateChangedHandle = EventManager->SubscribeToWeaponStateChanged(
            [this](FGameplayTag OldState, FGameplayTag NewState, bool bInterrupted)
            {
                OnWeaponStateChanged(OldState, NewState, bInterrupted);
            });
            
        // Subscribe to reload start events
        WeaponReloadStartHandle = EventManager->SubscribeToWeaponReloadStart(
            [this]()
            {
                OnWeaponReloadStart();
            });
            
        // Subscribe to reload end events
        WeaponReloadEndHandle = EventManager->SubscribeToWeaponReloadEnd(
            [this]()
            {
                OnWeaponReloadEnd();
            });
            
        // Subscribe to active weapon changed events
        ActiveWeaponChangedHandle = EventManager->SubscribeToActiveWeaponChanged(
            [this](AActor* NewWeapon)
            {
                OnActiveWeaponChanged(NewWeapon);
            });
            
        UE_LOG(LogTemp, Log, TEXT("[MedComWeaponUIWidget] Subscribed to events"));
    }
}

void UMedComWeaponUIWidget::UnsubscribeFromEvents()
{
    if (UEventDelegateManager* EventManager = UMedComBaseWidget::GetDelegateManager())
    {
        // Unsubscribe from all events
        if (AmmoChangedHandle.IsValid())
        {
            EventManager->UniversalUnsubscribe(AmmoChangedHandle);
            AmmoChangedHandle.Reset();
        }
        
        if (WeaponStateChangedHandle.IsValid())
        {
            EventManager->UniversalUnsubscribe(WeaponStateChangedHandle);
            WeaponStateChangedHandle.Reset();
        }
        
        if (WeaponReloadStartHandle.IsValid())
        {
            EventManager->UniversalUnsubscribe(WeaponReloadStartHandle);
            WeaponReloadStartHandle.Reset();
        }
        
        if (WeaponReloadEndHandle.IsValid())
        {
            EventManager->UniversalUnsubscribe(WeaponReloadEndHandle);
            WeaponReloadEndHandle.Reset();
        }
        
        if (ActiveWeaponChangedHandle.IsValid())
        {
            EventManager->UniversalUnsubscribe(ActiveWeaponChangedHandle);
            ActiveWeaponChangedHandle.Reset();
        }
        
        UE_LOG(LogTemp, Log, TEXT("[MedComWeaponUIWidget] Unsubscribed from events"));
    }
}

void UMedComWeaponUIWidget::UpdateFromWeaponInterfaces()
{
    if (!CachedWeaponActor)
        return;
        
    // Check if weapon implements the weapon interface
    if (CachedWeaponActor->Implements<UMedComWeaponInterface>())
    {
        // Get current ammo values from weapon
        float CurrentAmmo = IMedComWeaponInterface::Execute_GetCurrentAmmo(CachedWeaponActor);
        float RemainingAmmo = IMedComWeaponInterface::Execute_GetRemainingAmmo(CachedWeaponActor);
        float MagazineSize = IMedComWeaponInterface::Execute_GetMagazineSize(CachedWeaponActor);
        
        // Only update if values have changed
        if (LastCurrentAmmo != CurrentAmmo || 
            LastMagazineSize != MagazineSize || 
            LastRemainingAmmo != RemainingAmmo)
        {
            UpdateAmmoDisplay_Implementation(CurrentAmmo, RemainingAmmo, MagazineSize);
        }
    }
}

void UMedComWeaponUIWidget::SetWeaponIcon(UTexture2D* Icon)
{
    if (!WeaponIcon)
        return;
    
    if (Icon && CachedWeaponActor)
    {
        WeaponIcon->SetBrushFromTexture(Icon);
        WeaponIcon->SetVisibility(ESlateVisibility::Visible);
        
        // Set icon size based on texture dimensions
        FVector2D IconSize(Icon->GetSizeX(), Icon->GetSizeY());
        WeaponIcon->SetDesiredSizeOverride(IconSize);
        
        UE_LOG(LogTemp, Log, TEXT("[MedComWeaponUIWidget] Weapon icon set: %dx%d"), 
            Icon->GetSizeX(), Icon->GetSizeY());
    }
    else
    {
        WeaponIcon->SetVisibility(ESlateVisibility::Hidden);
    }
}

void UMedComWeaponUIWidget::UpdateAmmoTextStyle(float CurrentAmmo, float MaxAmmo)
{
    if (!CurrentAmmoText)
        return;
    
    FLinearColor TextColor = NormalAmmoColor;
    
    // Determine color based on ammo thresholds
    if (CurrentAmmo <= CriticalAmmoThreshold)
    {
        TextColor = CriticalAmmoColor;
    }
    else if (CurrentAmmo <= LowAmmoThreshold)
    {
        TextColor = LowAmmoColor;
    }
    
    // Apply color to text
    CurrentAmmoText->SetColorAndOpacity(FSlateColor(TextColor));
}

void UMedComWeaponUIWidget::ResetWeaponDisplay()
{
    // Hide all weapon UI elements
    if (WeaponNameText) WeaponNameText->SetVisibility(ESlateVisibility::Hidden);
    if (CurrentAmmoText) CurrentAmmoText->SetVisibility(ESlateVisibility::Hidden);
    if (MaxAmmoText) MaxAmmoText->SetVisibility(ESlateVisibility::Hidden);
    if (RemainingAmmoText) RemainingAmmoText->SetVisibility(ESlateVisibility::Hidden);
    if (FireModeText) FireModeText->SetVisibility(ESlateVisibility::Hidden);
    if (WeaponIcon) WeaponIcon->SetVisibility(ESlateVisibility::Hidden);
    if (ReloadProgressBar) ReloadProgressBar->SetVisibility(ESlateVisibility::Hidden);
    
    UE_LOG(LogTemp, Log, TEXT("[MedComWeaponUIWidget] Weapon display reset"));
}

void UMedComWeaponUIWidget::UpdateCurrentFireMode()
{
    if (!CachedWeaponActor)
        return;
    
    // Set default fire mode if none is set
    if (!CurrentFireMode.IsValid())
    {
        FGameplayTag SingleFireMode = FGameplayTag::RequestGameplayTag(TEXT("Weapon.FireMode.Single"));
        UpdateFireMode_Implementation(SingleFireMode, FText::FromString(TEXT("SINGLE")));
    }
}

bool UMedComWeaponUIWidget::ValidateWidgetBindings() const
{
    bool bValid = true;
    
    // Check required widget bindings
    if (!CurrentAmmoText)
    {
        UE_LOG(LogTemp, Error, TEXT("[MedComWeaponUIWidget] CurrentAmmoText not bound"));
        bValid = false;
    }
    
    if (!MaxAmmoText)
    {
        UE_LOG(LogTemp, Error, TEXT("[MedComWeaponUIWidget] MaxAmmoText not bound"));
        bValid = false;
    }
    
    if (!RemainingAmmoText)
    {
        UE_LOG(LogTemp, Error, TEXT("[MedComWeaponUIWidget] RemainingAmmoText not bound"));
        bValid = false;
    }
    
    return bValid;
}

// Event handlers - these are called when events are received from the EventDelegateManager
void UMedComWeaponUIWidget::OnAmmoChanged(float CurrentAmmo, float RemainingAmmo, float MagazineSize)
{
    // Simply update the display - no event broadcasting!
    UpdateAmmoDisplay_Implementation(CurrentAmmo, RemainingAmmo, MagazineSize);
}

void UMedComWeaponUIWidget::OnWeaponStateChanged(FGameplayTag OldState, FGameplayTag NewState, bool bInterrupted)
{
    UpdateWeaponState_Implementation(NewState, true);
}

void UMedComWeaponUIWidget::OnWeaponReloadStart()
{
    // Show reload indicator with estimated time
    float EstimatedReloadTime = 3.0f; // Default reload time
    ShowReloadIndicator_Implementation(EstimatedReloadTime, 0.0f);
}

void UMedComWeaponUIWidget::OnWeaponReloadEnd()
{
    HideReloadIndicator_Implementation();
}

void UMedComWeaponUIWidget::OnActiveWeaponChanged(AActor* NewWeapon)
{
    SetWeaponInternal(NewWeapon);
}