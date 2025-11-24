// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/Weapon/IMedComWeaponInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UEventDelegateManager* IMedComWeaponInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
{
    if (!WorldContextObject)
    {
        return nullptr;
    }
    
    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        return nullptr;
    }
    
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }
    
    return GameInstance->GetSubsystem<UEventDelegateManager>();
}

void IMedComWeaponInterface::BroadcastWeaponFired(
    const UObject* Weapon,
    const FVector& Origin,
    const FVector& Impact,
    bool bSuccess,
    FName ShotType)
{
    if (!Weapon)
    {
        return;
    }
    
    if (UEventDelegateManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        Manager->NotifyWeaponFired(Origin, Impact, bSuccess, ShotType);
    }
}

void IMedComWeaponInterface::BroadcastAmmoChanged(
    const UObject* Weapon,
    float CurrentAmmo,
    float RemainingAmmo,
    float MagazineSize)
{
    if (!Weapon)
    {
        return;
    }
    
    if (UEventDelegateManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        Manager->NotifyAmmoChanged(CurrentAmmo, RemainingAmmo, MagazineSize);
    }
}

void IMedComWeaponInterface::BroadcastReloadStarted(
    const UObject* Weapon,
    float ReloadDuration)
{
    if (!Weapon)
    {
        return;
    }
    
    if (UEventDelegateManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        Manager->NotifyWeaponReloadStart();
    }
}

void IMedComWeaponInterface::BroadcastReloadCompleted(
    const UObject* Weapon,
    bool bSuccess)
{
    if (!Weapon)
    {
        return;
    }
    
    if (UEventDelegateManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        Manager->NotifyWeaponReloadEnd();
    }
}

void IMedComWeaponInterface::BroadcastFireModeChanged(
    const UObject* Weapon,
    const FGameplayTag& NewFireMode)
{
    if (!Weapon)
    {
        return;
    }
    
    if (UEventDelegateManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        // Получаем текущий разброс из интерфейса оружия
        float CurrentSpread = 0.0f;
        if (const IMedComWeaponInterface* WeaponInterface = Cast<IMedComWeaponInterface>(Weapon))
        {
            CurrentSpread = WeaponInterface->Execute_GetCurrentSpread(Weapon);
        }
        
        Manager->NotifyFireModeChanged(NewFireMode, CurrentSpread);
    }
}