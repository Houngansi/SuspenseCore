// Copyright MedCom Team. All Rights Reserved.

#include "Components/WeaponAmmoComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "Interfaces/Weapon/IMedComWeaponInterface.h"
#include "ItemSystem/MedComItemManager.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Components/EquipmentAttributeComponent.h"
#include "Attributes/MedComWeaponAttributeSet.h"
#include "Attributes/MedComAmmoAttributeSet.h"
#include "AbilitySystemGlobals.h"

UWeaponAmmoComponent::UWeaponAmmoComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    
    // Initialize runtime state
    AmmoState = FInventoryAmmoState();
    bIsReloading = false;
    ReloadStartTime = 0.0f;
    bIsTacticalReload = true;
    
    // Initialize cached references
    LinkedAttributeComponent = nullptr;
    CachedWeaponAttributeSet = nullptr;
    CachedAmmoAttributeSet = nullptr;
    CachedMagazineSize = 30.0f;
    bMagazineSizeCached = false;
}

void UWeaponAmmoComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // Try to find and link attribute component on the same actor
    if (AActor* Owner = GetOwner())
    {
        if (UEquipmentAttributeComponent* AttrComp = Owner->FindComponentByClass<UEquipmentAttributeComponent>())
        {
            LinkAttributeComponent(AttrComp);
        }
    }
    
    EQUIPMENT_LOG(Verbose, TEXT("WeaponAmmoComponent initialized"));
}

void UWeaponAmmoComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Only replicate runtime state
    DOREPLIFETIME(UWeaponAmmoComponent, AmmoState);
    DOREPLIFETIME(UWeaponAmmoComponent, bIsReloading);
    DOREPLIFETIME(UWeaponAmmoComponent, ReloadStartTime);
    DOREPLIFETIME(UWeaponAmmoComponent, bIsTacticalReload);
}

void UWeaponAmmoComponent::Cleanup()
{
    // Cancel any active reload
    if (bIsReloading)
    {
        CancelReload();
    }
    
    // Clear cached references
    CachedWeaponInterface = nullptr;
    LinkedAttributeComponent = nullptr;
    CachedWeaponAttributeSet = nullptr;
    CachedAmmoAttributeSet = nullptr;
    bMagazineSizeCached = false;
    
    Super::Cleanup();
    
    EQUIPMENT_LOG(Verbose, TEXT("WeaponAmmoComponent cleaned up"));
}

bool UWeaponAmmoComponent::InitializeFromWeapon(TScriptInterface<IMedComWeaponInterface> WeaponInterface)
{
    if (!WeaponInterface)
    {
        EQUIPMENT_LOG(Error, TEXT("InitializeFromWeapon: Invalid weapon interface"));
        return false;
    }
    
    CachedWeaponInterface = WeaponInterface;
    
    // Get initial ammo state from weapon
    AmmoState = IMedComWeaponInterface::Execute_GetAmmoState(WeaponInterface.GetObject());
    
    // Update cached magazine size from attributes
    UpdateMagazineSizeFromAttributes();
    
    // If no saved state, initialize with full magazine
    if (!AmmoState.bHasAmmoState)
    {
        float MagazineSize = GetMagazineSize();
        AmmoState.CurrentAmmo = MagazineSize;
        AmmoState.RemainingAmmo = MagazineSize * 3; // Default reserve ammo
        AmmoState.AmmoType = GetAmmoType();
        AmmoState.bHasAmmoState = true;
        
        EQUIPMENT_LOG(Log, TEXT("Initialized with default ammo: %.0f/%.0f"), 
            AmmoState.CurrentAmmo, AmmoState.RemainingAmmo);
    }
    
    // Initial broadcast
    BroadcastAmmoChanged();
    
    return true;
}

void UWeaponAmmoComponent::LinkAttributeComponent(UEquipmentAttributeComponent* AttributeComponent)
{
    LinkedAttributeComponent = AttributeComponent;
    
    if (LinkedAttributeComponent)
    {
        // Cache AttributeSets for performance
        CachedWeaponAttributeSet = Cast<UMedComWeaponAttributeSet>(LinkedAttributeComponent->GetWeaponAttributeSet());
        CachedAmmoAttributeSet = Cast<UMedComAmmoAttributeSet>(LinkedAttributeComponent->GetAmmoAttributeSet());
        
        // Invalidate cache to force update
        bMagazineSizeCached = false;
        
        EQUIPMENT_LOG(Log, TEXT("Linked to attribute component - WeaponAS: %s, AmmoAS: %s"), 
            CachedWeaponAttributeSet ? TEXT("Valid") : TEXT("Null"),
            CachedAmmoAttributeSet ? TEXT("Valid") : TEXT("Null"));
    }
}

UMedComWeaponAttributeSet* UWeaponAmmoComponent::GetWeaponAttributeSet() const
{
    // Return cached if available
    if (CachedWeaponAttributeSet)
    {
        return CachedWeaponAttributeSet;
    }
    
    // Try to get from linked component
    if (LinkedAttributeComponent)
    {
        return Cast<UMedComWeaponAttributeSet>(LinkedAttributeComponent->GetWeaponAttributeSet());
    }
    
    // Try to find from ASC on owner
    if (AActor* Owner = GetOwner())
    {
        if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
        {
            for (UAttributeSet* Set : ASC->GetSpawnedAttributes())
            {
                if (UMedComWeaponAttributeSet* WeaponSet = Cast<UMedComWeaponAttributeSet>(Set))
                {
                    return WeaponSet;
                }
            }
        }
    }
    
    return nullptr;
}

UMedComAmmoAttributeSet* UWeaponAmmoComponent::GetAmmoAttributeSet() const
{
    // Return cached if available
    if (CachedAmmoAttributeSet)
    {
        return CachedAmmoAttributeSet;
    }
    
    // Try to get from linked component
    if (LinkedAttributeComponent)
    {
        return Cast<UMedComAmmoAttributeSet>(LinkedAttributeComponent->GetAmmoAttributeSet());
    }
    
    // Try to find from ASC on owner
    if (AActor* Owner = GetOwner())
    {
        if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
        {
            for (UAttributeSet* Set : ASC->GetSpawnedAttributes())
            {
                if (UMedComAmmoAttributeSet* AmmoSet = Cast<UMedComAmmoAttributeSet>(Set))
                {
                    return AmmoSet;
                }
            }
        }
    }
    
    return nullptr;
}

bool UWeaponAmmoComponent::ConsumeAmmo(float Amount)
{
    if (!ExecuteOnServer("ConsumeAmmo", [&]() {}))
    {
        return false;
    }
    
    if (Amount <= 0.0f)
    {
        EQUIPMENT_LOG(Warning, TEXT("ConsumeAmmo: Invalid amount: %.1f"), Amount);
        return false;
    }
    
    // Проверяем наличие патронов
    if (AmmoState.CurrentAmmo < Amount)
    {
        EQUIPMENT_LOG(Verbose, TEXT("ConsumeAmmo: Insufficient ammo (%.1f < %.1f)"), 
            AmmoState.CurrentAmmo, Amount);
        return false;
    }
    
    // Расходуем патроны
    AmmoState.CurrentAmmo -= Amount;
    
    // Применяем эффекты износа
    ApplyDurabilityModifiers();
    
    // Сохраняем изменения для персистентности
    SaveAmmoStateToWeapon();
    
    // Уведомляем об изменении
    BroadcastAmmoChanged();
    
    EQUIPMENT_LOG(Verbose, TEXT("Consumed %.1f ammo, %.1f remaining in magazine"), 
        Amount, AmmoState.CurrentAmmo);
    
    return true;
}

void UWeaponAmmoComponent::SaveAmmoStateToWeapon()
{
    // Получаем интерфейс оружия для доступа к ItemInstance
    IMedComWeaponInterface* WeaponInterface = GetWeaponInterface();
    if (!WeaponInterface)
    {
        // Нет оружия - нечего сохранять
        return;
    }
    
    // Вызываем SetAmmoState на оружии ТОЛЬКО для сохранения в ItemInstance
    // Оружие НЕ должно вызывать обратно SetAmmoState на компоненте
    IMedComWeaponInterface::Execute_SetAmmoState(Cast<UObject>(WeaponInterface), AmmoState);
    
    EQUIPMENT_LOG(Verbose, TEXT("SaveAmmoStateToWeapon: Persisted ammo state %.1f/%.1f"), 
        AmmoState.CurrentAmmo, AmmoState.RemainingAmmo);
}

float UWeaponAmmoComponent::AddAmmo(float Amount)
{
    if (!ExecuteOnServer("AddAmmo", [&]() {}))
    {
        return 0.0f;
    }
    
    if (Amount <= 0.0f)
    {
        return 0.0f;
    }
    
    float OldRemaining = AmmoState.RemainingAmmo;
    AmmoState.RemainingAmmo += Amount;
    
    float ActuallyAdded = AmmoState.RemainingAmmo - OldRemaining;
    
    if (ActuallyAdded > 0.0f)
    {
        // Сохраняем изменения
        SaveAmmoStateToWeapon();
        
        // Уведомляем об изменении
        BroadcastAmmoChanged();
        
        EQUIPMENT_LOG(Log, TEXT("Added %.1f ammo to reserve, total: %.1f"), 
            ActuallyAdded, AmmoState.RemainingAmmo);
    }
    
    return ActuallyAdded;
}

bool UWeaponAmmoComponent::StartReload(bool bForce)
{
    if (!ExecuteOnServer("StartReload", [&]() {
        ServerStartReload(bForce);
    }))
    {
        return true; // Client prediction
    }
    
    // Check if already reloading
    if (bIsReloading)
    {
        EQUIPMENT_LOG(Verbose, TEXT("Already reloading"));
        return false;
    }
    
    // Check if reload needed
    if (!bForce && (IsMagazineFull() || AmmoState.RemainingAmmo <= 0.0f))
    {
        EQUIPMENT_LOG(Verbose, TEXT("Reload not needed"));
        return false;
    }
    
    // Determine reload type
    bIsTacticalReload = AmmoState.CurrentAmmo > 0.0f;
    
    // Start reload
    bIsReloading = true;
    ReloadStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    
    // Apply reload effect
    ApplyReloadEffect();
    
    // Broadcast reload start
    if (UEventDelegateManager* Manager = GetDelegateManager())
    {
        Manager->NotifyWeaponReloadStart();
    }
    
    float ReloadDuration = GetReloadTime(bIsTacticalReload);
    EQUIPMENT_LOG(Log, TEXT("%s reload started, duration: %.1fs"), 
        bIsTacticalReload ? TEXT("Tactical") : TEXT("Full"), ReloadDuration);
    
    return true;
}

void UWeaponAmmoComponent::CompleteReload()
{
    if (!ExecuteOnServer("CompleteReload", [&]() {
        ServerCompleteReload();
    }))
    {
        return;
    }
    
    if (!bIsReloading)
    {
        EQUIPMENT_LOG(Warning, TEXT("CompleteReload called but not reloading"));
        return;
    }
    
    // Вычисляем количество патронов для перезарядки
    float MagazineSize = GetMagazineSize();
    float AmmoNeeded = MagazineSize - AmmoState.CurrentAmmo;
    float AmmoToTransfer = FMath::Min(AmmoNeeded, AmmoState.RemainingAmmo);
    
    // Переносим патроны
    AmmoState.CurrentAmmo += AmmoToTransfer;
    AmmoState.RemainingAmmo -= AmmoToTransfer;
    
    // Завершаем перезарядку
    bIsReloading = false;
    ReloadStartTime = 0.0f;
    
    // Удаляем эффект перезарядки
    if (ReloadEffectHandle.IsValid() && CachedASC)
    {
        CachedASC->RemoveActiveGameplayEffect(ReloadEffectHandle);
        ReloadEffectHandle.Invalidate();
    }
    
    // Сохраняем новое состояние
    SaveAmmoStateToWeapon();
    
    // Уведомляем о завершении перезарядки
    if (UEventDelegateManager* Manager = GetDelegateManager())
    {
        Manager->NotifyWeaponReloadEnd();
    }
    
    BroadcastAmmoChanged();
    
    EQUIPMENT_LOG(Log, TEXT("Reload completed: transferred %.1f ammo, magazine: %.1f/%.1f"), 
        AmmoToTransfer, AmmoState.CurrentAmmo, MagazineSize);
}

void UWeaponAmmoComponent::CancelReload()
{
    if (!bIsReloading)
    {
        return;
    }
    
    bIsReloading = false;
    ReloadStartTime = 0.0f;
    
    // Remove reload effect
    if (ReloadEffectHandle.IsValid() && CachedASC)
    {
        CachedASC->RemoveActiveGameplayEffect(ReloadEffectHandle);
        ReloadEffectHandle.Invalidate();
    }
    
    // Broadcast cancel
    if (UEventDelegateManager* Manager = GetDelegateManager())
    {
        Manager->NotifyWeaponReloadEnd();
    }
    
    EQUIPMENT_LOG(Log, TEXT("Reload cancelled"));
}

void UWeaponAmmoComponent::SetAmmoState(const FInventoryAmmoState& NewState)
{
    if (!ExecuteOnServer("SetAmmoState", [&]() {}))
    {
        return;
    }
    
    // Обновляем внутреннее состояние
    AmmoState = NewState;
    
    // Сохраняем для персистентности
    SaveAmmoStateToWeapon();
    
    // Уведомляем подписчиков об изменении
    BroadcastAmmoChanged();
    
    EQUIPMENT_LOG(Log, TEXT("Ammo state set: %.1f/%.1f"), 
        AmmoState.CurrentAmmo, AmmoState.RemainingAmmo);
}

bool UWeaponAmmoComponent::CanReload() const
{
    if (bIsReloading)
    {
        return false;
    }
    
    if (IsMagazineFull())
    {
        return false;
    }
    
    return AmmoState.RemainingAmmo > 0.0f;
}

bool UWeaponAmmoComponent::HasAmmo() const
{
    return AmmoState.CurrentAmmo > 0.0f;
}

bool UWeaponAmmoComponent::IsMagazineFull() const
{
    float MagazineSize = GetMagazineSize();
    return AmmoState.IsMagazineFull(MagazineSize);
}
void UWeaponAmmoComponent::UpdateInternalAmmoState(const FInventoryAmmoState& NewState)
{
    // Обновляем состояние без вызова внешних интерфейсов
    AmmoState = NewState;
    
    // Только broadcast изменения
    BroadcastAmmoChanged();
    
    EQUIPMENT_LOG(Verbose, TEXT("Internal ammo state updated: %.1f/%.1f"), 
        AmmoState.CurrentAmmo, AmmoState.RemainingAmmo);
}
float UWeaponAmmoComponent::GetMagazineSize() const
{
    // Return cached value if valid
    if (bMagazineSizeCached)
    {
        return CachedMagazineSize;
    }
    
    // First priority: Get from WeaponAttributeSet
    if (UMedComWeaponAttributeSet* WeaponAS = GetWeaponAttributeSet())
    {
        CachedMagazineSize = WeaponAS->GetMagazineSize();
        bMagazineSizeCached = true;
        return CachedMagazineSize;
    }
    
    // Second priority: Get from AmmoAttributeSet (for special ammo types that modify magazine)
    if (UMedComAmmoAttributeSet* AmmoAS = GetAmmoAttributeSet())
    {
        float AmmoMagazineSize = AmmoAS->GetMagazineSize();
        if (AmmoMagazineSize > 0.0f)
        {
            CachedMagazineSize = AmmoMagazineSize;
            bMagazineSizeCached = true;
            return CachedMagazineSize;
        }
    }
    
    // Fallback: Get base values from DataTable for weapon archetype
    FMedComUnifiedItemData WeaponData;
    if (GetWeaponData(WeaponData))
    {
        // Check weapon archetype for default values
        if (WeaponData.WeaponArchetype.MatchesTag(FGameplayTag::RequestGameplayTag("Weapon.Type.Ranged.AssaultRifle")))
        {
            CachedMagazineSize = 30.0f;
        }
        else if (WeaponData.WeaponArchetype.MatchesTag(FGameplayTag::RequestGameplayTag("Weapon.Type.Ranged.SMG")))
        {
            CachedMagazineSize = 25.0f;
        }
        else if (WeaponData.WeaponArchetype.MatchesTag(FGameplayTag::RequestGameplayTag("Weapon.Type.Ranged.LMG")))
        {
            CachedMagazineSize = 100.0f;
        }
        else if (WeaponData.WeaponArchetype.MatchesTag(FGameplayTag::RequestGameplayTag("Weapon.Type.Ranged.SniperRifle")))
        {
            CachedMagazineSize = 10.0f;
        }
        else if (WeaponData.WeaponArchetype.MatchesTag(FGameplayTag::RequestGameplayTag("Weapon.Type.Ranged.Shotgun")))
        {
            CachedMagazineSize = 8.0f;
        }
        else if (WeaponData.WeaponArchetype.MatchesTag(FGameplayTag::RequestGameplayTag("Weapon.Type.Ranged.Pistol")))
        {
            CachedMagazineSize = 15.0f;
        }
        else
        {
            CachedMagazineSize = 30.0f; // Default
        }
        
        bMagazineSizeCached = true;
        return CachedMagazineSize;
    }
    
    // Ultimate fallback
    EQUIPMENT_LOG(Warning, TEXT("GetMagazineSize: Failed to get magazine size from any source, using default"));
    return 30.0f;
}

float UWeaponAmmoComponent::GetReloadTime(bool bTactical) const
{
    // First priority: Get from WeaponAttributeSet
    if (UMedComWeaponAttributeSet* WeaponAS = GetWeaponAttributeSet())
    {
        if (bTactical)
        {
            return WeaponAS->GetTacticalReloadTime();
        }
        else
        {
            return WeaponAS->GetFullReloadTime();
        }
    }
    
    // Second priority: Get from AmmoAttributeSet (special ammo might affect reload)
    if (UMedComAmmoAttributeSet* AmmoAS = GetAmmoAttributeSet())
    {
        float ReloadTimeModifier = AmmoAS->GetReloadTime();
        if (ReloadTimeModifier > 0.0f)
        {
            // AmmoAS stores a modifier, not absolute time
            float BaseTime = bTactical ? 2.5f : 3.5f;
            return BaseTime * ReloadTimeModifier;
        }
    }
    
    // Fallback: Get base values from weapon archetype
    FMedComUnifiedItemData WeaponData;
    if (GetWeaponData(WeaponData))
    {
        float BaseReloadTime = bTactical ? 2.5f : 3.5f;
        
        // Adjust by weapon type
        if (WeaponData.WeaponArchetype.MatchesTag(FGameplayTag::RequestGameplayTag("Weapon.Type.Ranged.LMG")))
        {
            BaseReloadTime *= 1.5f; // LMGs reload slower
        }
        else if (WeaponData.WeaponArchetype.MatchesTag(FGameplayTag::RequestGameplayTag("Weapon.Type.Ranged.Pistol")))
        {
            BaseReloadTime *= 0.7f; // Pistols reload faster
        }
        else if (WeaponData.WeaponArchetype.MatchesTag(FGameplayTag::RequestGameplayTag("Weapon.Type.Ranged.Shotgun")))
        {
            // Shotguns reload shell by shell
            float MagazineSize = GetMagazineSize();
            float AmmoToLoad = bTactical ? (MagazineSize - AmmoState.CurrentAmmo) : MagazineSize;
            return 0.5f * AmmoToLoad; // 0.5s per shell
        }
        
        return BaseReloadTime;
    }
    
    // Ultimate fallback
    return bTactical ? 2.5f : 3.5f;
}

FGameplayTag UWeaponAmmoComponent::GetAmmoType() const
{
    // Get from weapon data
    FMedComUnifiedItemData WeaponData;
    if (GetWeaponData(WeaponData))
    {
        return WeaponData.AmmoType;
    }
    
    return FGameplayTag::EmptyTag;
}

void UWeaponAmmoComponent::UpdateMagazineSizeFromAttributes()
{
    // Invalidate cache to force recalculation
    bMagazineSizeCached = false;
    
    // Update cached values
    float NewMagazineSize = GetMagazineSize();
    
    // If magazine size changed and we have more ammo than new size, adjust
    if (AmmoState.CurrentAmmo > NewMagazineSize)
    {
        float Excess = AmmoState.CurrentAmmo - NewMagazineSize;
        AmmoState.CurrentAmmo = NewMagazineSize;
        AmmoState.RemainingAmmo += Excess;
        
        EQUIPMENT_LOG(Log, TEXT("Magazine size reduced, moved %.1f ammo to reserve"), Excess);
    }
}

void UWeaponAmmoComponent::ApplyDurabilityModifiers()
{
    // Check weapon durability and apply malfunction chances
    if (UMedComWeaponAttributeSet* WeaponAS = GetWeaponAttributeSet())
    {
        float Durability = WeaponAS->GetDurability();
        float MaxDurability = WeaponAS->GetMaxDurability();
        float DurabilityPercent = MaxDurability > 0.0f ? Durability / MaxDurability : 1.0f;
        
        // Low durability increases misfire chance
        if (DurabilityPercent < 0.5f && FMath::RandRange(0.0f, 1.0f) < WeaponAS->GetMisfireChance() / 100.0f)
        {
            // Misfire occurred - broadcast event
            if (UEventDelegateManager* Manager = GetDelegateManager())
            {
                FGameplayEventData Payload;
                Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.Weapon.Misfire");
                Payload.EventMagnitude = DurabilityPercent;
                
                // Handle misfire event through GAS
                if (CachedASC)
                {
                    CachedASC->HandleGameplayEvent(Payload.EventTag, &Payload);
                }
            }
            
            EQUIPMENT_LOG(Warning, TEXT("Weapon misfire due to low durability: %.1f%%"), DurabilityPercent * 100.0f);
        }
    }
}

IMedComWeaponInterface* UWeaponAmmoComponent::GetWeaponInterface() const
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

bool UWeaponAmmoComponent::GetWeaponData(FMedComUnifiedItemData& OutData) const
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

void UWeaponAmmoComponent::BroadcastAmmoChanged()
{
    float MagazineSize = GetMagazineSize();
    
    // Call base class method to broadcast
    UEquipmentComponentBase::BroadcastAmmoChanged(
        AmmoState.CurrentAmmo, 
        AmmoState.RemainingAmmo, 
        MagazineSize
    );
}

void UWeaponAmmoComponent::ApplyReloadEffect()
{
    if (!CachedASC)
    {
        return;
    }
    
    // Get reload effect from weapon data
    FMedComUnifiedItemData WeaponData;
    if (!GetWeaponData(WeaponData))
    {
        return;
    }
    
    // Find reload effect in passive effects
    for (const TSubclassOf<UGameplayEffect>& EffectClass : WeaponData.PassiveEffects)
    {
        if (!EffectClass)
        {
            continue;
        }
        
        // Check if this is reload effect by tag
        UGameplayEffect* CDO = EffectClass.GetDefaultObject();
        if (CDO && CDO->InheritableGameplayEffectTags.CombinedTags.HasTag(
            FGameplayTag::RequestGameplayTag(TEXT("Effect.Weapon.Reload"))))
        {
            // Apply effect
            FGameplayEffectContextHandle Context = CachedASC->MakeEffectContext();
            Context.AddSourceObject(this);
            
            FGameplayEffectSpecHandle Spec = CachedASC->MakeOutgoingSpec(
                EffectClass, 
                1.0f, 
                Context
            );
            
            if (Spec.IsValid())
            {
                // Set reload duration based on reload type
                float ReloadDuration = GetReloadTime(bIsTacticalReload);
                Spec.Data->SetSetByCallerMagnitude(
                    FGameplayTag::RequestGameplayTag(TEXT("Data.Duration")), 
                    ReloadDuration
                );
                
                // Add tags to identify reload type
                if (bIsTacticalReload)
                {
                    Spec.Data->DynamicGrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Weapon.Reloading.Tactical")));
                }
                else
                {
                    Spec.Data->DynamicGrantedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Weapon.Reloading.Full")));
                }
                
                ReloadEffectHandle = CachedASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
                
                EQUIPMENT_LOG(Verbose, TEXT("Applied %s reload effect for %.1fs"), 
                    bIsTacticalReload ? TEXT("tactical") : TEXT("full"), ReloadDuration);
            }
        }
    }
}

void UWeaponAmmoComponent::OnRep_AmmoState()
{
    BroadcastAmmoChanged();
    
    EQUIPMENT_LOG(Verbose, TEXT("OnRep_AmmoState: %.1f/%.1f"), 
        AmmoState.CurrentAmmo, AmmoState.RemainingAmmo);
}

void UWeaponAmmoComponent::OnRep_ReloadState()
{
    if (bIsReloading)
    {
        if (UEventDelegateManager* Manager = GetDelegateManager())
        {
            Manager->NotifyWeaponReloadStart();
        }
    }
    else
    {
        if (UEventDelegateManager* Manager = GetDelegateManager())
        {
            Manager->NotifyWeaponReloadEnd();
        }
    }
    
    EQUIPMENT_LOG(Verbose, TEXT("OnRep_ReloadState: %s"), 
        bIsReloading ? TEXT("Reloading") : TEXT("Not reloading"));
}

void UWeaponAmmoComponent::ServerStartReload_Implementation(bool bForce)
{
    StartReload(bForce);
}

bool UWeaponAmmoComponent::ServerStartReload_Validate(bool bForce)
{
    return true;
}

void UWeaponAmmoComponent::ServerCompleteReload_Implementation()
{
    CompleteReload();
}

bool UWeaponAmmoComponent::ServerCompleteReload_Validate()
{
    return true;
}