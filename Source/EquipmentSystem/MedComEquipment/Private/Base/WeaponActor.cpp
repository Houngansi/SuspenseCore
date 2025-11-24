// Copyright MedCom Team. All Rights Reserved.

#include "Base/WeaponActor.h"

#include "Components/WeaponAmmoComponent.h"
#include "Components/WeaponFireModeComponent.h"
#include "Components/EquipmentMeshComponent.h"
#include "Components/EquipmentAttributeComponent.h"
#include "ItemSystem/MedComItemManager.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"

// Logging
DEFINE_LOG_CATEGORY_STATIC(LogWeaponActor, Log, All);

namespace WeaponDefaults
{
    static constexpr float DefaultDamage      = 25.0f;
    static constexpr float DefaultFireRate    = 600.0f;
    static constexpr float DefaultReloadTime  = 2.5f;
    static constexpr float DefaultRecoil      = 1.0f;
    static constexpr float DefaultRange       = 10000.0f;

    // Runtime property keys (persist only via ItemInstance)
    static const FName Prop_CurrentAmmo     (TEXT("CurrentAmmo"));
    static const FName Prop_RemainingAmmo   (TEXT("RemainingAmmo"));
    static const FName Prop_CurrentFireMode (TEXT("CurrentFireMode")); // index stored as float
}

AWeaponActor::AWeaponActor()
{
    PrimaryActorTick.bCanEverTick = false;

    AmmoComponent     = CreateDefaultSubobject<UWeaponAmmoComponent>(TEXT("AmmoComponent"));
    FireModeComponent = CreateDefaultSubobject<UWeaponFireModeComponent>(TEXT("FireModeComponent"));
    ScopeCamera       = CreateDefaultSubobject<UCameraComponent>(TEXT("ScopeCamera"));

    if (ScopeCamera)
    {
        // В UE5 RootComponent — TObjectPtr; для SetupAttachment нужен сырой указатель.
        ScopeCamera->SetupAttachment(RootComponent.Get()); // допустимо и если RootComponent == nullptr
        ScopeCamera->bAutoActivate = false;
    }

    bReplicates = true;
    bNetUseOwnerRelevancy = true;
}

void AWeaponActor::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogWeaponActor, Verbose, TEXT("WeaponActor BeginPlay: %s"), *GetName());
}

void AWeaponActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Persist ammo / firemode (component already persists on changes; here — final guard)
    SaveWeaponState();

    // Soft cleanup components (no GA/GE touch)
    if (AmmoComponent)     { AmmoComponent->Cleanup(); }
    if (FireModeComponent) { FireModeComponent->Cleanup(); }

    Super::EndPlay(EndPlayReason);
}

UEventDelegateManager* AWeaponActor::GetDelegateManager() const
{
    // У этого фасада собственного менеджера нет — события маршрутизируются компонентами/сервисами.
    return nullptr;
}

//================================================
// AEquipmentActorBase override: extend item-equip path
//================================================
void AWeaponActor::OnItemInstanceEquipped_Implementation(const FInventoryItemInstance& ItemInstance)
{
    // Base: caches ASC, initializes Mesh/Attribute/Attachment from SSOT + fires UI.Equipment.DataReady
    Super::OnItemInstanceEquipped_Implementation(ItemInstance);

    // Load SSOT data for weapon specifics
    if (UMedComItemManager* ItemManager = GetItemManager())
    {
        if (ItemManager->GetUnifiedItemData(ItemInstance.ItemID, CachedItemData))
        {
            bHasCachedData = true;

            if (!CachedItemData.bIsWeapon)
            {
                UE_LOG(LogWeaponActor, Error, TEXT("Item '%s' is not a weapon in SSOT"), *ItemInstance.ItemID.ToString());
                return;
            }

            // Initialize owned weapon components from SSOT (ONLY public APIs of mesh component)
            SetupComponentsFromItemData(CachedItemData);

            // Restore persisted runtime bits (ammo / fire mode index)
            RestoreWeaponState();

            UE_LOG(LogWeaponActor, Log, TEXT("Weapon initialized from SSOT: %s"), *CachedItemData.DisplayName.ToString());
            return;
        }
    }

    UE_LOG(LogWeaponActor, Error, TEXT("OnItemInstanceEquipped: failed to read SSOT for ItemID=%s"), *ItemInstance.ItemID.ToString());
}

//================================================
// IMedComWeaponInterface (facade)
//================================================
FWeaponInitializationResult AWeaponActor::InitializeFromItemData_Implementation(const FInventoryItemInstance& ItemInstance)
{
    FWeaponInitializationResult R;

    // Use the same unified path as equip (no GA/GE/Attach here)
    OnItemInstanceEquipped_Implementation(ItemInstance);

    R.bSuccess         = bHasCachedData;
    R.FireModesLoaded  = FireModeComponent ? IMedComFireModeProviderInterface::Execute_GetAvailableFireModeCount(FireModeComponent) : 0;
    R.AbilitiesGranted = 0; // actor grants nothing

    if (!R.bSuccess)
    {
        R.ErrorMessage = FText::FromString(TEXT("Failed to initialize weapon from SSOT"));
    }
    return R;
}

bool AWeaponActor::GetWeaponItemData_Implementation(FMedComUnifiedItemData& OutData) const
{
    if (bHasCachedData)
    {
        OutData = CachedItemData;
        return true;
    }
    return false;
}

FInventoryItemInstance AWeaponActor::GetItemInstance_Implementation() const
{
    return EquippedItemInstance;
}

bool AWeaponActor::Fire_Implementation(const FWeaponFireParams& /*Params*/)
{
    // Actor doesn't simulate fire; ability flow does it.
    return AmmoComponent ? AmmoComponent->ConsumeAmmo(1.0f) : false;
}

void AWeaponActor::StopFire_Implementation()
{
    // Intentionally empty (handled by abilities / components)
}

bool AWeaponActor::Reload_Implementation(bool bForce)
{
    return AmmoComponent ? AmmoComponent->StartReload(bForce) : false;
}

void AWeaponActor::CancelReload_Implementation()
{
    if (AmmoComponent) { AmmoComponent->CancelReload(); }
}

FGameplayTag AWeaponActor::GetWeaponArchetype_Implementation() const
{
    return bHasCachedData ? CachedItemData.WeaponArchetype : FGameplayTag::EmptyTag;
}

FGameplayTag AWeaponActor::GetWeaponType_Implementation() const
{
    return bHasCachedData ? CachedItemData.ItemType : FGameplayTag::EmptyTag;
}

FGameplayTag AWeaponActor::GetAmmoType_Implementation() const
{
    return bHasCachedData ? CachedItemData.AmmoType : FGameplayTag::EmptyTag;
}

FName AWeaponActor::GetMuzzleSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.MuzzleSocket : NAME_None;
}

FName AWeaponActor::GetSightSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.SightSocket : NAME_None;
}

FName AWeaponActor::GetMagazineSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.MagazineSocket : NAME_None;
}

FName AWeaponActor::GetGripSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.GripSocket : NAME_None;
}

FName AWeaponActor::GetStockSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.StockSocket : NAME_None;
}

float AWeaponActor::GetWeaponDamage_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("Damage"), WeaponDefaults::DefaultDamage);
}

float AWeaponActor::GetFireRate_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("FireRate"), WeaponDefaults::DefaultFireRate);
}

float AWeaponActor::GetReloadTime_Implementation() const
{
    // Delegated to AmmoComponent where possible
    return AmmoComponent ? AmmoComponent->GetReloadTime(/*bTactical*/ true) : WeaponDefaults::DefaultReloadTime;
}

float AWeaponActor::GetRecoil_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("Recoil"), WeaponDefaults::DefaultRecoil);
}

float AWeaponActor::GetRange_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("Range"), WeaponDefaults::DefaultRange);
}

float AWeaponActor::GetBaseSpread_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("BaseSpread"), 0.0f);
}

float AWeaponActor::GetMaxSpread_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("MaxSpread"), 0.0f);
}

float AWeaponActor::GetCurrentSpread_Implementation() const
{
    // Actor does not simulate dynamic spread anymore; return base value for UI if needed.
    return GetBaseSpread_Implementation();
}

void AWeaponActor::SetCurrentSpread_Implementation(float /*NewSpread*/)
{
    // No-op: spread simulation is handled by abilities/components
}

float AWeaponActor::GetCurrentAmmo_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetCurrentAmmo() : 0.0f;
}

float AWeaponActor::GetRemainingAmmo_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetRemainingAmmo() : 0.0f;
}

float AWeaponActor::GetMagazineSize_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetMagazineSize() : 0.0f;
}

FInventoryAmmoState AWeaponActor::GetAmmoState_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetAmmoState() : FInventoryAmmoState();
}

void AWeaponActor::SetAmmoState_Implementation(const FInventoryAmmoState& NewState)
{
    // IMPORTANT: actor only persists state to ItemInstance, not pushes into component (to avoid recursion)
    if (!EquippedItemInstance.IsValid())
    {
        return;
    }

    EquippedItemInstance.SetRuntimeProperty(WeaponDefaults::Prop_CurrentAmmo,     NewState.CurrentAmmo);
    EquippedItemInstance.SetRuntimeProperty(WeaponDefaults::Prop_RemainingAmmo,   NewState.RemainingAmmo);
}

bool AWeaponActor::CanReload_Implementation() const
{
    return AmmoComponent ? AmmoComponent->CanReload() : false;
}

bool AWeaponActor::IsMagazineFull_Implementation() const
{
    return AmmoComponent ? AmmoComponent->IsMagazineFull() : true;
}

FWeaponStateFlags AWeaponActor::GetWeaponState_Implementation() const
{
    FWeaponStateFlags Flags;
    if (AmmoComponent)
    {
        Flags.bIsReloading = AmmoComponent->IsReloading();
    }
    // bIsFiring / bIsAiming handled by abilities/components
    return Flags;
}

bool AWeaponActor::IsInWeaponState_Implementation(const FWeaponStateFlags& State) const
{
    const FWeaponStateFlags Cur = GetWeaponState_Implementation();
    if (State.bIsReloading && !Cur.bIsReloading) return false;
    if (State.bIsFiring    && !Cur.bIsFiring)    return false;
    if (State.bIsAiming    && !Cur.bIsAiming)    return false;
    return true;
}

void AWeaponActor::SetWeaponState_Implementation(const FWeaponStateFlags& NewState, bool bEnabled)
{
    // Let components handle the real state transitions
    if (NewState.bIsReloading && AmmoComponent)
    {
        if (bEnabled)  { AmmoComponent->StartReload(false); }
        else           { AmmoComponent->CancelReload();     }
    }
}

//================================================
// IMedComFireModeProviderInterface (proxy -> component)
//================================================
bool AWeaponActor::InitializeFromWeaponData_Implementation(const FMedComUnifiedItemData& WeaponData)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_InitializeFromWeaponData(FireModeComponent, WeaponData)
        : false;
}

void AWeaponActor::ClearFireModes_Implementation()
{
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
    {
        IMedComFireModeProviderInterface::Execute_ClearFireModes(FireModeComponent);
    }
}

bool AWeaponActor::CycleToNextFireMode_Implementation()
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_CycleToNextFireMode(FireModeComponent)
        : false;
}

bool AWeaponActor::CycleToPreviousFireMode_Implementation()
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_CycleToPreviousFireMode(FireModeComponent)
        : false;
}

bool AWeaponActor::SetFireMode_Implementation(const FGameplayTag& FireModeTag)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_SetFireMode(FireModeComponent, FireModeTag)
        : false;
}

bool AWeaponActor::SetFireModeByIndex_Implementation(int32 Index)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_SetFireModeByIndex(FireModeComponent, Index)
        : false;
}

FGameplayTag AWeaponActor::GetCurrentFireMode_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_GetCurrentFireMode(FireModeComponent)
        : FGameplayTag::EmptyTag;
}

FFireModeRuntimeData AWeaponActor::GetCurrentFireModeData_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_GetCurrentFireModeData(FireModeComponent)
        : FFireModeRuntimeData();
}

bool AWeaponActor::IsFireModeAvailable_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_IsFireModeAvailable(FireModeComponent, FireModeTag)
        : false;
}

TArray<FFireModeRuntimeData> AWeaponActor::GetAllFireModes_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_GetAllFireModes(FireModeComponent)
        : TArray<FFireModeRuntimeData>();
}

TArray<FGameplayTag> AWeaponActor::GetAvailableFireModes_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_GetAvailableFireModes(FireModeComponent)
        : TArray<FGameplayTag>();
}

int32 AWeaponActor::GetAvailableFireModeCount_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_GetAvailableFireModeCount(FireModeComponent)
        : 0;
}

bool AWeaponActor::SetFireModeEnabled_Implementation(const FGameplayTag& FireModeTag, bool bEnabled)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_SetFireModeEnabled(FireModeComponent, FireModeTag, bEnabled)
        : false;
}

void AWeaponActor::SetFireModeBlocked_Implementation(const FGameplayTag& FireModeTag, bool bBlocked)
{
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
    {
        IMedComFireModeProviderInterface::Execute_SetFireModeBlocked(FireModeComponent, FireModeTag, bBlocked);
    }
}

bool AWeaponActor::IsFireModeBlocked_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_IsFireModeBlocked(FireModeComponent, FireModeTag)
        : false;
}

bool AWeaponActor::GetFireModeData_Implementation(const FGameplayTag& FireModeTag, FFireModeRuntimeData& OutData) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_GetFireModeData(FireModeComponent, FireModeTag, OutData)
        : false;
}

TSubclassOf<UGameplayAbility> AWeaponActor::GetFireModeAbility_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_GetFireModeAbility(FireModeComponent, FireModeTag)
        : nullptr;
}

int32 AWeaponActor::GetFireModeInputID_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
        ? IMedComFireModeProviderInterface::Execute_GetFireModeInputID(FireModeComponent, FireModeTag)
        : INDEX_NONE;
}

//================================================
// Utility
//================================================
FVector AWeaponActor::GetMuzzleLocation() const
{
    const UEquipmentMeshComponent* MC = MeshComponent;
    if (MC)
    {
        const FName Socket = IMedComWeaponInterface::Execute_GetMuzzleSocketName(const_cast<AWeaponActor*>(this));
        if (Socket != NAME_None && MC->DoesSocketExist(Socket))
        {
            return MC->GetSocketLocation(Socket);
        }
    }
    return GetActorLocation();
}

FRotator AWeaponActor::GetMuzzleRotation() const
{
    const UEquipmentMeshComponent* MC = MeshComponent;
    if (MC)
    {
        const FName Socket = IMedComWeaponInterface::Execute_GetMuzzleSocketName(const_cast<AWeaponActor*>(this));
        if (Socket != NAME_None && MC->DoesSocketExist(Socket))
        {
            return MC->GetSocketRotation(Socket);
        }
    }
    return GetActorRotation();
}

FTransform AWeaponActor::GetMuzzleTransform() const
{
    const UEquipmentMeshComponent* MC = MeshComponent;
    if (MC)
    {
        const FName Socket = IMedComWeaponInterface::Execute_GetMuzzleSocketName(const_cast<AWeaponActor*>(this));
        if (Socket != NAME_None && MC->DoesSocketExist(Socket))
        {
            return MC->GetSocketTransform(Socket);
        }
    }
    return GetActorTransform();
}

void AWeaponActor::SaveWeaponState()
{
    if (!EquippedItemInstance.IsValid())
    {
        return;
    }

    // Persist ammo via interface contract (component already calls this on changes)
    if (AmmoComponent)
    {
        const FInventoryAmmoState AS = AmmoComponent->GetAmmoState();
        SetAmmoState_Implementation(AS);
    }

    // Persist fire mode index (for quick restore)
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
    {
        const TArray<FFireModeRuntimeData> All = IMedComFireModeProviderInterface::Execute_GetAllFireModes(FireModeComponent);
        const FGameplayTag Cur = IMedComFireModeProviderInterface::Execute_GetCurrentFireMode(FireModeComponent);

        const int32 Index = All.IndexOfByPredicate([&](const FFireModeRuntimeData& E){ return E.FireModeTag == Cur; });
        if (Index != INDEX_NONE)
        {
            EquippedItemInstance.SetRuntimeProperty(WeaponDefaults::Prop_CurrentFireMode, static_cast<float>(Index));
        }
    }
}

void AWeaponActor::RestoreWeaponState()
{
    if (!EquippedItemInstance.IsValid())
    {
        return;
    }

    // Restore ammo
    if (AmmoComponent)
    {
        const float Curr  = EquippedItemInstance.GetRuntimeProperty(WeaponDefaults::Prop_CurrentAmmo,   -1.0f);
        const float Rem   = EquippedItemInstance.GetRuntimeProperty(WeaponDefaults::Prop_RemainingAmmo, -1.0f);

        if (Curr >= 0.0f && Rem >= 0.0f)
        {
            FInventoryAmmoState S;
            S.CurrentAmmo    = Curr;
            S.RemainingAmmo  = Rem;
            S.AmmoType       = GetAmmoType_Implementation();
            S.bHasAmmoState  = true;
            AmmoComponent->SetAmmoState(S); // component handles broadcast + persist
        }
    }

    // Restore fire mode by saved index
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(UMedComFireModeProviderInterface::StaticClass()))
    {
        const float SavedIndexF = EquippedItemInstance.GetRuntimeProperty(WeaponDefaults::Prop_CurrentFireMode, -1.0f);
        if (SavedIndexF >= 0.0f)
        {
            IMedComFireModeProviderInterface::Execute_SetFireModeByIndex(FireModeComponent, FMath::RoundToInt(SavedIndexF));
        }
    }
}

//================================================
// Internal helpers
//================================================
void AWeaponActor::SetupComponentsFromItemData(const FMedComUnifiedItemData& ItemData)
{
    // Mesh: use ONLY public interface (InitializeFromItemInstance уже вызван базой в S3)
    if (UEquipmentMeshComponent* MC = MeshComponent)
    {
        MC->SetupWeaponVisuals(ItemData); // применяет оружейные визуальные настройки
    }

    // Link AttributeComponent to AmmoComponent for attribute access
    if (UEquipmentAttributeComponent* AC = AttributeComponent)
    {
        if (AmmoComponent)
        {
            AmmoComponent->LinkAttributeComponent(AC);
        }
    }

    // Create weapon interface handle for components
    TScriptInterface<IMedComWeaponInterface> SelfIface;
    SelfIface.SetObject(this);
    SelfIface.SetInterface(Cast<IMedComWeaponInterface>(this));

    // Initialize components from weapon (ASC cached in base at equip-time)
    if (AmmoComponent)
    {
        AmmoComponent->Initialize(GetOwner(), CachedASC);
        (void)AmmoComponent->InitializeFromWeapon(SelfIface);
    }

    if (FireModeComponent)
    {
        FireModeComponent->Initialize(GetOwner(), CachedASC);
        (void)FireModeComponent->InitializeFromWeapon(SelfIface);
    }
}

float AWeaponActor::GetWeaponAttributeValue(const FName& AttributeName, float DefaultValue) const
{
    if (const UEquipmentAttributeComponent* AC = AttributeComponent)
    {
        float V = 0.f;
        if (AC->GetAttributeValue(AttributeName.ToString(), V))
        {
            return V;
        }
    }
    return DefaultValue;
}
