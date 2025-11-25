// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/Weapon/ISuspenseWeapon.h"
#include "Delegates/SuspenseEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseEventManager* ISuspenseWeapon::GetDelegateManagerStatic(const UObject* WorldContextObject)
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
    
    return GameInstance->GetSubsystem<USuspenseEventManager>();
}

void ISuspenseWeapon::BroadcastWeaponFired(
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
    
    if (USuspenseEventManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        Manager->NotifyWeaponFired(Origin, Impact, bSuccess, ShotType);
    }
}

void ISuspenseWeapon::BroadcastAmmoChanged(
    const UObject* Weapon,
    float CurrentAmmo,
    float RemainingAmmo,
    float MagazineSize)
{
    if (!Weapon)
    {
        return;
    }
    
    if (USuspenseEventManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        Manager->NotifyAmmoChanged(CurrentAmmo, RemainingAmmo, MagazineSize);
    }
}

void ISuspenseWeapon::BroadcastReloadStarted(
    const UObject* Weapon,
    float ReloadDuration)
{
    if (!Weapon)
    {
        return;
    }
    
    if (USuspenseEventManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        Manager->NotifyWeaponReloadStart();
    }
}

void ISuspenseWeapon::BroadcastReloadCompleted(
    const UObject* Weapon,
    bool bSuccess)
{
    if (!Weapon)
    {
        return;
    }
    
    if (USuspenseEventManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        Manager->NotifyWeaponReloadEnd();
    }
}

void ISuspenseWeapon::BroadcastFireModeChanged(
    const UObject* Weapon,
    const FGameplayTag& NewFireMode)
{
    if (!Weapon)
    {
        return;
    }
    
    if (USuspenseEventManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        // Получаем текущий разброс из интерфейса оружия
        float CurrentSpread = 0.0f;
        if (const ISuspenseWeapon* WeaponInterface = Cast<ISuspenseWeapon>(Weapon))
        {
            CurrentSpread = WeaponInterface->Execute_GetCurrentSpread(Weapon);
        }
        
        Manager->NotifyFireModeChanged(NewFireMode, CurrentSpread);
    }
}