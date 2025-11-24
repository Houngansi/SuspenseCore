// Copyright MedCom Team. All Rights Reserved.

#include "Components/WeaponFireModeComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "Net/UnrealNetwork.h"
#include "Interfaces/Weapon/IMedComWeaponInterface.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"

UWeaponFireModeComponent::UWeaponFireModeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    
    // Initialize runtime state
    CurrentFireModeIndex = 0;
    bIsSwitching = false;
}

void UWeaponFireModeComponent::BeginPlay()
{
    Super::BeginPlay();
    
    EQUIPMENT_LOG(Log, TEXT("WeaponFireModeComponent initialized"));
}

void UWeaponFireModeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Only replicate the current index - everything else comes from DataTable
    DOREPLIFETIME(UWeaponFireModeComponent, CurrentFireModeIndex);
}

void UWeaponFireModeComponent::Cleanup()
{
    // Remove granted abilities
    RemoveFireModeAbilities();
    
    // Clear runtime state
    FireModes.Empty();
    BlockedFireModes.Empty();
    AbilityHandles.Empty();
    CurrentFireModeIndex = 0;
    CachedWeaponInterface = nullptr;
    
    Super::Cleanup();
    
    EQUIPMENT_LOG(Log, TEXT("WeaponFireModeComponent cleaned up"));
}

bool UWeaponFireModeComponent::InitializeFromWeapon(TScriptInterface<IMedComWeaponInterface> WeaponInterface)
{
    if (!WeaponInterface)
    {
        EQUIPMENT_LOG(Error, TEXT("InitializeFromWeapon: Invalid weapon interface"));
        return false;
    }
    
    CachedWeaponInterface = WeaponInterface;
    
    // Get weapon data from interface using Execute_ version
    FMedComUnifiedItemData WeaponData;
    bool bGotData = false;
    
    // Проверяем, что объект реализует интерфейс
    if (WeaponInterface.GetObject() && WeaponInterface.GetObject()->GetClass()->ImplementsInterface(UMedComWeaponInterface::StaticClass()))
    {
        bGotData = IMedComWeaponInterface::Execute_GetWeaponItemData(WeaponInterface.GetObject(), WeaponData);
    }
    else
    {
        EQUIPMENT_LOG(Error, TEXT("InitializeFromWeapon: Object does not implement IMedComWeaponInterface"));
        return false;
    }
    
    if (!bGotData)
    {
        EQUIPMENT_LOG(Error, TEXT("InitializeFromWeapon: Failed to get weapon data from interface"));
        return false;
    }
    
    // Initialize from weapon data
    return InitializeFromWeaponData(WeaponData);
}

bool UWeaponFireModeComponent::InitializeFromWeaponData_Implementation(const FMedComUnifiedItemData& WeaponData)
{
    if (!WeaponData.bIsWeapon)
    {
        EQUIPMENT_LOG(Error, TEXT("InitializeFromWeaponData: Item is not a weapon"));
        return false;
    }
    
    // ИСПРАВЛЕНО: Используем Execute_ версию для вызова функции интерфейса
    // Это критически важно для BlueprintNativeEvent функций
    IMedComFireModeProviderInterface::Execute_ClearFireModes(this);
    
    // Load fire modes from DataTable
    LoadFireModesFromData(WeaponData);
    
    if (FireModes.Num() == 0)
    {
        EQUIPMENT_LOG(Error, TEXT("InitializeFromWeaponData: No fire modes found in weapon data"));
        return false;
    }
    
    // Set default fire mode
    int32 DefaultIndex = 0;
    if (WeaponData.DefaultFireMode.IsValid())
    {
        for (int32 i = 0; i < FireModes.Num(); ++i)
        {
            if (FireModes[i].FireModeTag == WeaponData.DefaultFireMode)
            {
                DefaultIndex = i;
                break;
            }
        }
    }
    
    CurrentFireModeIndex = DefaultIndex;
    FireModes[CurrentFireModeIndex].bIsActive = true;
    
    // Grant abilities
    GrantFireModeAbilities();
    
    // Initial broadcast
    BroadcastFireModeChanged();
    
    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: используем Execute_ версию для вызова метода интерфейса
    EQUIPMENT_LOG(Log, TEXT("Initialized with %d fire modes, default: %s"), 
        FireModes.Num(), 
        *IMedComFireModeProviderInterface::Execute_GetCurrentFireMode(this).ToString());
    
    return true;
}
void UWeaponFireModeComponent::ClearFireModes_Implementation()
{
    RemoveFireModeAbilities();
    FireModes.Empty();
    BlockedFireModes.Empty();
    CurrentFireModeIndex = 0;
}

bool UWeaponFireModeComponent::CycleToNextFireMode_Implementation()
{
    if (FireModes.Num() <= 1)
    {
        return false;
    }
    
    // Find next available fire mode
    int32 StartIndex = CurrentFireModeIndex;
    int32 NextIndex = (CurrentFireModeIndex + 1) % FireModes.Num();
    
    while (NextIndex != StartIndex)
    {
        if (FireModes[NextIndex].bIsAvailable && !IsFireModeBlocked(FireModes[NextIndex].FireModeTag))
        {
            return SetFireModeByIndex(NextIndex);
        }
        
        NextIndex = (NextIndex + 1) % FireModes.Num();
    }
    
    return false;
}

bool UWeaponFireModeComponent::CycleToPreviousFireMode_Implementation()
{
    if (FireModes.Num() <= 1)
    {
        return false;
    }
    
    // Find previous available fire mode
    int32 StartIndex = CurrentFireModeIndex;
    int32 PrevIndex = (CurrentFireModeIndex - 1 + FireModes.Num()) % FireModes.Num();
    
    while (PrevIndex != StartIndex)
    {
        if (FireModes[PrevIndex].bIsAvailable && !IsFireModeBlocked(FireModes[PrevIndex].FireModeTag))
        {
            return SetFireModeByIndex(PrevIndex);
        }
        
        PrevIndex = (PrevIndex - 1 + FireModes.Num()) % FireModes.Num();
    }
    
    return false;
}

bool UWeaponFireModeComponent::SetFireMode_Implementation(const FGameplayTag& FireModeTag)
{
    int32 Index = FindFireModeIndex(FireModeTag);
    if (Index == INDEX_NONE)
    {
        EQUIPMENT_LOG(Warning, TEXT("SetFireMode: Fire mode not found: %s"), *FireModeTag.ToString());
        return false;
    }
    
    return SetFireModeByIndex(Index);
}

bool UWeaponFireModeComponent::SetFireModeByIndex_Implementation(int32 Index)
{
    // Validate index
    if (!FireModes.IsValidIndex(Index))
    {
        EQUIPMENT_LOG(Warning, TEXT("SetFireModeByIndex: Invalid index %d"), Index);
        return false;
    }
    
    // Check if already active
    if (Index == CurrentFireModeIndex)
    {
        return true;
    }
    
    // Check availability
    const FFireModeRuntimeData& NewMode = FireModes[Index];
    if (!NewMode.bIsAvailable || IsFireModeBlocked(NewMode.FireModeTag))
    {
        EQUIPMENT_LOG(Warning, TEXT("SetFireModeByIndex: Fire mode not available: %s"), 
            *NewMode.FireModeTag.ToString());
        return false;
    }
    
    // Prevent recursion
    if (bIsSwitching)
    {
        return false;
    }
    
    TGuardValue<bool> SwitchGuard(bIsSwitching, true);
    
    // Update state
    if (FireModes.IsValidIndex(CurrentFireModeIndex))
    {
        FireModes[CurrentFireModeIndex].bIsActive = false;
    }
    
    CurrentFireModeIndex = Index;
    FireModes[CurrentFireModeIndex].bIsActive = true;
    
    // Broadcast change
    BroadcastFireModeChanged();
    
    EQUIPMENT_LOG(Log, TEXT("Fire mode changed to: %s"), *NewMode.FireModeTag.ToString());
    
    return true;
}

FGameplayTag UWeaponFireModeComponent::GetCurrentFireMode_Implementation() const
{
    if (FireModes.IsValidIndex(CurrentFireModeIndex))
    {
        return FireModes[CurrentFireModeIndex].FireModeTag;
    }
    
    return FGameplayTag::EmptyTag;
}

FFireModeRuntimeData UWeaponFireModeComponent::GetCurrentFireModeData_Implementation() const
{
    if (FireModes.IsValidIndex(CurrentFireModeIndex))
    {
        return FireModes[CurrentFireModeIndex];
    }
    
    return FFireModeRuntimeData();
}

bool UWeaponFireModeComponent::IsFireModeAvailable_Implementation(const FGameplayTag& FireModeTag) const
{
    int32 Index = FindFireModeIndex(FireModeTag);
    if (Index == INDEX_NONE)
    {
        return false;
    }
    
    return FireModes[Index].bIsAvailable && !IsFireModeBlocked(FireModeTag);
}

TArray<FFireModeRuntimeData> UWeaponFireModeComponent::GetAllFireModes_Implementation() const
{
    return FireModes;
}

TArray<FGameplayTag> UWeaponFireModeComponent::GetAvailableFireModes_Implementation() const
{
    TArray<FGameplayTag> Available;
    
    for (const FFireModeRuntimeData& Mode : FireModes)
    {
        if (Mode.bIsAvailable && !IsFireModeBlocked(Mode.FireModeTag))
        {
            Available.Add(Mode.FireModeTag);
        }
    }
    
    return Available;
}

int32 UWeaponFireModeComponent::GetAvailableFireModeCount_Implementation() const
{
    int32 Count = 0;
    
    for (const FFireModeRuntimeData& Mode : FireModes)
    {
        if (Mode.bIsAvailable && !IsFireModeBlocked(Mode.FireModeTag))
        {
            Count++;
        }
    }
    
    return Count;
}

bool UWeaponFireModeComponent::SetFireModeEnabled_Implementation(const FGameplayTag& FireModeTag, bool bEnabled)
{
    int32 Index = FindFireModeIndex(FireModeTag);
    if (Index == INDEX_NONE)
    {
        return false;
    }
    
    FireModes[Index].bIsAvailable = bEnabled;
    
    // Broadcast availability change
    IMedComFireModeProviderInterface::BroadcastFireModeAvailabilityChanged(this, FireModeTag, bEnabled);
    
    // If disabling current mode, switch to another
    if (!bEnabled && Index == CurrentFireModeIndex)
    {
        CycleToNextFireMode();
    }
    
    return true;
}

void UWeaponFireModeComponent::SetFireModeBlocked_Implementation(const FGameplayTag& FireModeTag, bool bBlocked)
{
    if (bBlocked)
    {
        BlockedFireModes.Add(FireModeTag);
    }
    else
    {
        BlockedFireModes.Remove(FireModeTag);
    }
    
    // If blocking current mode, switch
    if (bBlocked && GetCurrentFireMode() == FireModeTag)
    {
        CycleToNextFireMode();
    }
}

bool UWeaponFireModeComponent::IsFireModeBlocked_Implementation(const FGameplayTag& FireModeTag) const
{
    return BlockedFireModes.Contains(FireModeTag);
}

bool UWeaponFireModeComponent::GetFireModeData_Implementation(const FGameplayTag& FireModeTag, FFireModeRuntimeData& OutData) const
{
    int32 Index = FindFireModeIndex(FireModeTag);
    if (Index == INDEX_NONE)
    {
        return false;
    }
    
    OutData = FireModes[Index];
    return true;
}

TSubclassOf<UGameplayAbility> UWeaponFireModeComponent::GetFireModeAbility_Implementation(const FGameplayTag& FireModeTag) const
{
    int32 Index = FindFireModeIndex(FireModeTag);
    if (Index != INDEX_NONE)
    {
        return FireModes[Index].FireModeAbility;
    }
    
    return nullptr;
}

int32 UWeaponFireModeComponent::GetFireModeInputID_Implementation(const FGameplayTag& FireModeTag) const
{
    int32 Index = FindFireModeIndex(FireModeTag);
    if (Index != INDEX_NONE)
    {
        return FireModes[Index].InputID;
    }
    
    return INDEX_NONE;
}

UEventDelegateManager* UWeaponFireModeComponent::GetDelegateManager() const
{
    return Super::GetDelegateManager();
}

IMedComWeaponInterface* UWeaponFireModeComponent::GetWeaponInterface() const
{
    if (CachedWeaponInterface)
    {
        return Cast<IMedComWeaponInterface>(CachedWeaponInterface.GetInterface());
    }
    
    // Try to get from owner
    if (AActor* Owner = GetOwner())
    {
        if (Owner->GetClass()->ImplementsInterface(UMedComWeaponInterface::StaticClass()))
        {
            return Cast<IMedComWeaponInterface>(Owner);
        }
    }
    
    return nullptr;
}

bool UWeaponFireModeComponent::GetWeaponData(FMedComUnifiedItemData& OutData) const
{
    IMedComWeaponInterface* WeaponInterface = GetWeaponInterface();
    if (!WeaponInterface)
    {
        return false;
    }
    
    return IMedComWeaponInterface::Execute_GetWeaponItemData(
        Cast<UObject>(WeaponInterface), 
        OutData
    );
}

void UWeaponFireModeComponent::LoadFireModesFromData(const FMedComUnifiedItemData& WeaponData)
{
    FireModes.Empty();
    FireModes.Reserve(WeaponData.FireModes.Num());
    
    // Convert DataTable fire modes to runtime data
    for (int32 i = 0; i < WeaponData.FireModes.Num(); ++i)
    {
        const FWeaponFireModeData& DataTableMode = WeaponData.FireModes[i];
        
        // Create runtime data
        FFireModeRuntimeData RuntimeMode(DataTableMode, i);
        FireModes.Add(RuntimeMode);
        
        EQUIPMENT_LOG(Verbose, TEXT("Loaded fire mode: %s (Input: %d, Enabled: %s)"), 
            *RuntimeMode.FireModeTag.ToString(),
            RuntimeMode.InputID,
            RuntimeMode.bEnabled ? TEXT("Yes") : TEXT("No"));
    }
}

void UWeaponFireModeComponent::GrantFireModeAbilities()
{
    if (!CachedASC)
    {
        EQUIPMENT_LOG(Warning, TEXT("GrantFireModeAbilities: No ASC available"));
        return;
    }
    
    // Grant ability for each fire mode
    for (const FFireModeRuntimeData& Mode : FireModes)
    {
        if (!Mode.FireModeAbility)
        {
            continue;
        }
        
        // Skip if already granted
        if (AbilityHandles.Contains(Mode.FireModeTag))
        {
            continue;
        }
        
        // Grant ability
        FGameplayAbilitySpec AbilitySpec(
            Mode.FireModeAbility,
            1,
            Mode.InputID,
            this
        );
        
        FGameplayAbilitySpecHandle Handle = CachedASC->GiveAbility(AbilitySpec);
        
        if (Handle.IsValid())
        {
            AbilityHandles.Add(Mode.FireModeTag, Handle);
            
            EQUIPMENT_LOG(Log, TEXT("Granted ability for fire mode: %s"), 
                *Mode.FireModeTag.ToString());
        }
        else
        {
            EQUIPMENT_LOG(Error, TEXT("Failed to grant ability for fire mode: %s"), 
                *Mode.FireModeTag.ToString());
        }
    }
}

void UWeaponFireModeComponent::RemoveFireModeAbilities()
{
    if (!CachedASC)
    {
        return;
    }
    
    // Remove all granted abilities
    for (const auto& Pair : AbilityHandles)
    {
        if (Pair.Value.IsValid())
        {
            CachedASC->ClearAbility(Pair.Value);
        }
    }
    
    AbilityHandles.Empty();
    
    EQUIPMENT_LOG(Log, TEXT("Removed all fire mode abilities"));
}

int32 UWeaponFireModeComponent::FindFireModeIndex(const FGameplayTag& FireModeTag) const
{
    for (int32 i = 0; i < FireModes.Num(); ++i)
    {
        if (FireModes[i].FireModeTag == FireModeTag)
        {
            return i;
        }
    }
    
    return INDEX_NONE;
}

void UWeaponFireModeComponent::BroadcastFireModeChanged()
{
    if (!FireModes.IsValidIndex(CurrentFireModeIndex))
    {
        return;
    }
    
    const FFireModeRuntimeData& CurrentMode = FireModes[CurrentFireModeIndex];
    
    // Get current spread from weapon
    float CurrentSpread = 0.0f;
    if (IMedComWeaponInterface* WeaponInterface = GetWeaponInterface())
    {
        CurrentSpread = IMedComWeaponInterface::Execute_GetCurrentSpread(
            Cast<UObject>(WeaponInterface)
        );
    }
    
    // Broadcast through interface helper
    IMedComFireModeProviderInterface::BroadcastFireModeChanged(
        this,
        CurrentMode.FireModeTag,
        CurrentSpread
    );
}

void UWeaponFireModeComponent::OnRep_CurrentFireModeIndex()
{
    // Update active states
    for (int32 i = 0; i < FireModes.Num(); ++i)
    {
        FireModes[i].bIsActive = (i == CurrentFireModeIndex);
    }
    
    // Broadcast change on client
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        BroadcastFireModeChanged();
    }
    
    EQUIPMENT_LOG(Verbose, TEXT("OnRep_CurrentFireModeIndex: %d"), CurrentFireModeIndex);
}