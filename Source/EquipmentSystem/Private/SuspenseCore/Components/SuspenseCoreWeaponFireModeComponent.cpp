// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreWeaponFireModeComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "Net/UnrealNetwork.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/World.h"

USuspenseCoreWeaponFireModeComponent::USuspenseCoreWeaponFireModeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    // Initialize runtime state
    CurrentFireModeIndex = 0;
    bIsSwitching = false;
}

void USuspenseCoreWeaponFireModeComponent::BeginPlay()
{
    Super::BeginPlay();

    EQUIPMENT_LOG(Log, TEXT("WeaponFireModeComponent initialized"));
}

void USuspenseCoreWeaponFireModeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Only replicate the current index - everything else comes from DataTable
    DOREPLIFETIME(USuspenseCoreWeaponFireModeComponent, CurrentFireModeIndex);
}

void USuspenseCoreWeaponFireModeComponent::Cleanup()
{
    // Remove fire mode tag from ASC before cleanup
    if (FireModes.IsValidIndex(CurrentFireModeIndex))
    {
        UpdateFireModeTagOnASC(FireModes[CurrentFireModeIndex].FireModeTag, FGameplayTag::EmptyTag);
    }

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

bool USuspenseCoreWeaponFireModeComponent::InitializeFromWeapon(TScriptInterface<ISuspenseCoreWeapon> WeaponInterface)
{
    if (!WeaponInterface)
    {
        EQUIPMENT_LOG(Error, TEXT("InitializeFromWeapon: Invalid weapon interface"));
        return false;
    }

    CachedWeaponInterface = WeaponInterface;

    // Get weapon data from interface using Execute_ version
    FSuspenseCoreUnifiedItemData WeaponData;
    bool bGotData = false;

    // Проверяем, что объект реализует интерфейс
    if (WeaponInterface.GetObject() && WeaponInterface.GetObject()->GetClass()->ImplementsInterface(USuspenseCoreWeapon::StaticClass()))
    {
        bGotData = ISuspenseCoreWeapon::Execute_GetWeaponItemData(WeaponInterface.GetObject(), WeaponData);
    }
    else
    {
        EQUIPMENT_LOG(Error, TEXT("InitializeFromWeapon: Object does not implement ISuspenseCoreWeapon"));
        return false;
    }

    if (!bGotData)
    {
        EQUIPMENT_LOG(Error, TEXT("InitializeFromWeapon: Failed to get weapon data from interface"));
        return false;
    }

    // Initialize from weapon data - MUST use Execute_ for BlueprintNativeEvent
    return ISuspenseCoreFireModeProvider::Execute_InitializeFromWeaponData(this, WeaponData);
}

bool USuspenseCoreWeaponFireModeComponent::InitializeFromWeaponData_Implementation(const FSuspenseCoreUnifiedItemData& WeaponData)
{
    if (!WeaponData.bIsWeapon)
    {
        EQUIPMENT_LOG(Error, TEXT("InitializeFromWeaponData: Item is not a weapon"));
        return false;
    }

    // ИСПРАВЛЕНО: Используем Execute_ версию для вызова функции интерфейса
    // Это критически важно для BlueprintNativeEvent функций
    ISuspenseCoreFireModeProvider::Execute_ClearFireModes(this);

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

    // Add initial fire mode tag to owner's ASC
    // This enables ActivationRequiredTags for fire mode abilities (Single/Burst/Auto)
    UpdateFireModeTagOnASC(FGameplayTag::EmptyTag, FireModes[CurrentFireModeIndex].FireModeTag);

    // Initial broadcast
    BroadcastFireModeChanged();

    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: используем Execute_ версию для вызова метода интерфейса
    EQUIPMENT_LOG(Log, TEXT("Initialized with %d fire modes, default: %s"),
        FireModes.Num(),
        *ISuspenseCoreFireModeProvider::Execute_GetCurrentFireMode(this).ToString());

    return true;
}
void USuspenseCoreWeaponFireModeComponent::ClearFireModes_Implementation()
{
    RemoveFireModeAbilities();
    FireModes.Empty();
    BlockedFireModes.Empty();
    CurrentFireModeIndex = 0;
}

bool USuspenseCoreWeaponFireModeComponent::CycleToNextFireMode_Implementation()
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

bool USuspenseCoreWeaponFireModeComponent::CycleToPreviousFireMode_Implementation()
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

bool USuspenseCoreWeaponFireModeComponent::SetFireMode_Implementation(const FGameplayTag& FireModeTag)
{
    int32 Index = FindFireModeIndex(FireModeTag);
    if (Index == INDEX_NONE)
    {
        EQUIPMENT_LOG(Warning, TEXT("SetFireMode: Fire mode not found: %s"), *FireModeTag.ToString());
        return false;
    }

    return SetFireModeByIndex(Index);
}

bool USuspenseCoreWeaponFireModeComponent::SetFireModeByIndex_Implementation(int32 Index)
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
    const FSuspenseCoreFireModeRuntimeData& NewMode = FireModes[Index];
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

    // Cache old fire mode tag for ASC update
    FGameplayTag OldModeTag = FireModes.IsValidIndex(CurrentFireModeIndex)
        ? FireModes[CurrentFireModeIndex].FireModeTag
        : FGameplayTag::EmptyTag;

    // Update state
    if (FireModes.IsValidIndex(CurrentFireModeIndex))
    {
        FireModes[CurrentFireModeIndex].bIsActive = false;
    }

    CurrentFireModeIndex = Index;
    FireModes[CurrentFireModeIndex].bIsActive = true;

    // Update ASC tags - enables ActivationRequiredTags for fire mode abilities
    UpdateFireModeTagOnASC(OldModeTag, NewMode.FireModeTag);

    // Broadcast change
    BroadcastFireModeChanged();

    EQUIPMENT_LOG(Log, TEXT("Fire mode changed to: %s"), *NewMode.FireModeTag.ToString());

    return true;
}

FGameplayTag USuspenseCoreWeaponFireModeComponent::GetCurrentFireMode_Implementation() const
{
    if (FireModes.IsValidIndex(CurrentFireModeIndex))
    {
        return FireModes[CurrentFireModeIndex].FireModeTag;
    }

    return FGameplayTag::EmptyTag;
}

FSuspenseCoreFireModeRuntimeData USuspenseCoreWeaponFireModeComponent::GetCurrentFireModeData_Implementation() const
{
    if (FireModes.IsValidIndex(CurrentFireModeIndex))
    {
        return FireModes[CurrentFireModeIndex];
    }

    return FSuspenseCoreFireModeRuntimeData();
}

bool USuspenseCoreWeaponFireModeComponent::IsFireModeAvailable_Implementation(const FGameplayTag& FireModeTag) const
{
    int32 Index = FindFireModeIndex(FireModeTag);
    if (Index == INDEX_NONE)
    {
        return false;
    }

    return FireModes[Index].bIsAvailable && !IsFireModeBlocked(FireModeTag);
}

TArray<FSuspenseCoreFireModeRuntimeData> USuspenseCoreWeaponFireModeComponent::GetAllFireModes_Implementation() const
{
    return FireModes;
}

TArray<FGameplayTag> USuspenseCoreWeaponFireModeComponent::GetAvailableFireModes_Implementation() const
{
    TArray<FGameplayTag> Available;

    for (const FSuspenseCoreFireModeRuntimeData& Mode : FireModes)
    {
        if (Mode.bIsAvailable && !ISuspenseCoreFireModeProvider::Execute_IsFireModeBlocked(const_cast<USuspenseCoreWeaponFireModeComponent*>(this), Mode.FireModeTag))
        {
            Available.Add(Mode.FireModeTag);
        }
    }

    return Available;
}

int32 USuspenseCoreWeaponFireModeComponent::GetAvailableFireModeCount_Implementation() const
{
    int32 Count = 0;

    for (const FSuspenseCoreFireModeRuntimeData& Mode : FireModes)
    {
        if (Mode.bIsAvailable && !ISuspenseCoreFireModeProvider::Execute_IsFireModeBlocked(const_cast<USuspenseCoreWeaponFireModeComponent*>(this), Mode.FireModeTag))
        {
            Count++;
        }
    }

    return Count;
}

bool USuspenseCoreWeaponFireModeComponent::SetFireModeEnabled_Implementation(const FGameplayTag& FireModeTag, bool bEnabled)
{
    int32 Index = FindFireModeIndex(FireModeTag);
    if (Index == INDEX_NONE)
    {
        return false;
    }

    FireModes[Index].bIsAvailable = bEnabled;

    // Broadcast availability change
    ISuspenseCoreFireModeProvider::BroadcastFireModeAvailabilityChanged(this, FireModeTag, bEnabled);

    // If disabling current mode, switch to another
    if (!bEnabled && Index == CurrentFireModeIndex)
    {
        CycleToNextFireMode();
    }

    return true;
}

void USuspenseCoreWeaponFireModeComponent::SetFireModeBlocked_Implementation(const FGameplayTag& FireModeTag, bool bBlocked)
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

bool USuspenseCoreWeaponFireModeComponent::IsFireModeBlocked_Implementation(const FGameplayTag& FireModeTag) const
{
    return BlockedFireModes.Contains(FireModeTag);
}

bool USuspenseCoreWeaponFireModeComponent::GetFireModeData_Implementation(const FGameplayTag& FireModeTag, FSuspenseCoreFireModeRuntimeData& OutData) const
{
    int32 Index = FindFireModeIndex(FireModeTag);
    if (Index == INDEX_NONE)
    {
        return false;
    }

    OutData = FireModes[Index];
    return true;
}

TSubclassOf<UGameplayAbility> USuspenseCoreWeaponFireModeComponent::GetFireModeAbility_Implementation(const FGameplayTag& FireModeTag) const
{
    int32 Index = FindFireModeIndex(FireModeTag);
    if (Index != INDEX_NONE)
    {
        return FireModes[Index].FireModeAbility;
    }

    return nullptr;
}

int32 USuspenseCoreWeaponFireModeComponent::GetFireModeInputID_Implementation(const FGameplayTag& FireModeTag) const
{
    int32 Index = FindFireModeIndex(FireModeTag);
    if (Index != INDEX_NONE)
    {
        return FireModes[Index].InputID;
    }

    return INDEX_NONE;
}

USuspenseCoreEventManager* USuspenseCoreWeaponFireModeComponent::GetDelegateManager() const
{
    return Super::GetDelegateManager();
}

ISuspenseCoreWeapon* USuspenseCoreWeaponFireModeComponent::GetWeaponInterface() const
{
    if (CachedWeaponInterface)
    {
        return Cast<ISuspenseCoreWeapon>(CachedWeaponInterface.GetInterface());
    }

    // Try to get from owner
    if (AActor* Owner = GetOwner())
    {
        if (Owner->GetClass()->ImplementsInterface(USuspenseCoreWeapon::StaticClass()))
        {
            return Cast<ISuspenseCoreWeapon>(Owner);
        }
    }

    return nullptr;
}

bool USuspenseCoreWeaponFireModeComponent::GetWeaponData(FSuspenseCoreUnifiedItemData& OutData) const
{
    ISuspenseCoreWeapon* WeaponInterface = GetWeaponInterface();
    if (!WeaponInterface)
    {
        return false;
    }

    return ISuspenseCoreWeapon::Execute_GetWeaponItemData(
        Cast<UObject>(WeaponInterface),
        OutData
    );
}

void USuspenseCoreWeaponFireModeComponent::LoadFireModesFromData(const FSuspenseCoreUnifiedItemData& WeaponData)
{
    FireModes.Empty();
    FireModes.Reserve(WeaponData.FireModes.Num());

    // Convert DataTable fire modes to runtime data
    for (int32 i = 0; i < WeaponData.FireModes.Num(); ++i)
    {
        const FWeaponFireModeData& DataTableMode = WeaponData.FireModes[i];

        // Create runtime data
        FSuspenseCoreFireModeRuntimeData RuntimeMode(DataTableMode, i);
        FireModes.Add(RuntimeMode);

        EQUIPMENT_LOG(Verbose, TEXT("Loaded fire mode: %s (Input: %d, Enabled: %s)"),
            *RuntimeMode.FireModeTag.ToString(),
            RuntimeMode.InputID,
            RuntimeMode.bEnabled ? TEXT("Yes") : TEXT("No"));
    }
}

void USuspenseCoreWeaponFireModeComponent::GrantFireModeAbilities()
{
    if (!CachedASC)
    {
        EQUIPMENT_LOG(Warning, TEXT("GrantFireModeAbilities: No ASC available"));
        return;
    }

    // Grant ability for each fire mode
    for (const FSuspenseCoreFireModeRuntimeData& Mode : FireModes)
    {
        if (!Mode.FireModeAbility)
        {
            continue;
        }

        // Skip if already granted by this component
        if (AbilityHandles.Contains(Mode.FireModeTag))
        {
            continue;
        }

        // CRITICAL FIX: Check if ability of this class is already granted (e.g., by AttributeComponent)
        // This prevents duplicate abilities when GrantedAbilities in DataTable includes fire abilities
        if (CachedASC->FindAbilitySpecFromClass(Mode.FireModeAbility))
        {
            EQUIPMENT_LOG(Log, TEXT("Skipping fire mode ability (already granted): %s"),
                *Mode.FireModeTag.ToString());
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

void USuspenseCoreWeaponFireModeComponent::RemoveFireModeAbilities()
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

int32 USuspenseCoreWeaponFireModeComponent::FindFireModeIndex(const FGameplayTag& FireModeTag) const
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

void USuspenseCoreWeaponFireModeComponent::BroadcastFireModeChanged()
{
    if (!FireModes.IsValidIndex(CurrentFireModeIndex))
    {
        return;
    }

    const FSuspenseCoreFireModeRuntimeData& CurrentMode = FireModes[CurrentFireModeIndex];

    // Get current spread from weapon
    float CurrentSpread = 0.0f;
    if (ISuspenseCoreWeapon* WeaponInterface = GetWeaponInterface())
    {
        CurrentSpread = ISuspenseCoreWeapon::Execute_GetCurrentSpread(
            Cast<UObject>(WeaponInterface)
        );
    }

    // Broadcast through interface helper
    ISuspenseCoreFireModeProvider::BroadcastFireModeChanged(
        this,
        CurrentMode.FireModeTag,
        CurrentSpread
    );
}

void USuspenseCoreWeaponFireModeComponent::UpdateFireModeTagOnASC(const FGameplayTag& OldMode, const FGameplayTag& NewMode)
{
    // Get ASC from owner character (CachedASC is inherited from base component)
    UAbilitySystemComponent* ASC = CachedASC;
    if (!ASC)
    {
        // Try to get from owner if not cached
        AActor* Owner = GetOwner();
        if (Owner)
        {
            // Check if owner's owner has ASC (weapon -> character)
            if (AActor* CharacterActor = Owner->GetOwner())
            {
                ASC = CharacterActor->FindComponentByClass<UAbilitySystemComponent>();
            }
        }
    }

    if (!ASC)
    {
        EQUIPMENT_LOG(Warning, TEXT("UpdateFireModeTagOnASC: No ASC found, fire mode tags not updated"));
        return;
    }

    // Remove old fire mode tag
    if (OldMode.IsValid())
    {
        ASC->RemoveLooseGameplayTag(OldMode);
        EQUIPMENT_LOG(Verbose, TEXT("UpdateFireModeTagOnASC: Removed tag %s"), *OldMode.ToString());
    }

    // Add new fire mode tag - this enables ActivationRequiredTags for fire mode abilities
    if (NewMode.IsValid())
    {
        ASC->AddLooseGameplayTag(NewMode);
        EQUIPMENT_LOG(Log, TEXT("UpdateFireModeTagOnASC: Added tag %s"), *NewMode.ToString());
    }
}

void USuspenseCoreWeaponFireModeComponent::OnRep_CurrentFireModeIndex()
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
