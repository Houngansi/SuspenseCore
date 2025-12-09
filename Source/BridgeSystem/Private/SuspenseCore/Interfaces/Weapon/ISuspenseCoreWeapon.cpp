// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseCoreEventManager* ISuspenseCoreWeapon::GetDelegateManagerStatic(const UObject* WorldContextObject)
{
    if (!WorldContextObject)
    {
        return nullptr;
    }

    return USuspenseCoreEventManager::Get(WorldContextObject);
}

void ISuspenseCoreWeapon::BroadcastWeaponFired(
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

    if (USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        if (USuspenseCoreEventBus* EventBus = Manager->GetEventBus())
        {
            FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(const_cast<UObject*>(Weapon))
                .SetVector(TEXT("Origin"), Origin)
                .SetVector(TEXT("Impact"), Impact)
                .SetBool(TEXT("Success"), bSuccess)
                .SetString(TEXT("ShotType"), ShotType.ToString());

            EventBus->Publish(
                FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Weapon.Fired")),
                EventData
            );
        }
    }
}

void ISuspenseCoreWeapon::BroadcastAmmoChanged(
    const UObject* Weapon,
    float CurrentAmmo,
    float RemainingAmmo,
    float MagazineSize)
{
    if (!Weapon)
    {
        return;
    }

    if (USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        if (USuspenseCoreEventBus* EventBus = Manager->GetEventBus())
        {
            FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(const_cast<UObject*>(Weapon))
                .SetFloat(TEXT("CurrentAmmo"), CurrentAmmo)
                .SetFloat(TEXT("RemainingAmmo"), RemainingAmmo)
                .SetFloat(TEXT("MagazineSize"), MagazineSize);

            EventBus->Publish(
                FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Weapon.AmmoChanged")),
                EventData
            );
        }
    }
}

void ISuspenseCoreWeapon::BroadcastReloadStarted(
    const UObject* Weapon,
    float ReloadDuration)
{
    if (!Weapon)
    {
        return;
    }

    if (USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        if (USuspenseCoreEventBus* EventBus = Manager->GetEventBus())
        {
            FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(const_cast<UObject*>(Weapon))
                .SetFloat(TEXT("ReloadDuration"), ReloadDuration)
                .SetBool(TEXT("Started"), true);

            EventBus->Publish(
                FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Weapon.ReloadStarted")),
                EventData
            );
        }
    }
}

void ISuspenseCoreWeapon::BroadcastReloadCompleted(
    const UObject* Weapon,
    bool bSuccess)
{
    if (!Weapon)
    {
        return;
    }

    if (USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        if (USuspenseCoreEventBus* EventBus = Manager->GetEventBus())
        {
            FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(const_cast<UObject*>(Weapon))
                .SetBool(TEXT("Completed"), true)
                .SetBool(TEXT("Success"), bSuccess);

            EventBus->Publish(
                FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Weapon.ReloadCompleted")),
                EventData
            );
        }
    }
}

void ISuspenseCoreWeapon::BroadcastFireModeChanged(
    const UObject* Weapon,
    const FGameplayTag& NewFireMode)
{
    if (!Weapon)
    {
        return;
    }

    if (USuspenseCoreEventManager* Manager = GetDelegateManagerStatic(Weapon))
    {
        if (USuspenseCoreEventBus* EventBus = Manager->GetEventBus())
        {
            // Get current spread from weapon interface
            float CurrentSpread = 0.0f;
            if (Weapon->GetClass()->ImplementsInterface(USuspenseCoreWeapon::StaticClass()))
            {
                CurrentSpread = ISuspenseCoreWeapon::Execute_GetCurrentSpread(Weapon);
            }

            FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(const_cast<UObject*>(Weapon))
                .SetString(TEXT("FireModeTag"), NewFireMode.ToString())
                .SetFloat(TEXT("CurrentSpread"), CurrentSpread);

            EventBus->Publish(
                FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Weapon.FireModeChanged")),
                EventData
            );
        }
    }
}
