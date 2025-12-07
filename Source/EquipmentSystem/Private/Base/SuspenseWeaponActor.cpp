// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Base/SuspenseWeaponActor.h"

#include "SuspenseCore/Components/SuspenseWeaponAmmoComponent.h"
#include "SuspenseCore/Components/SuspenseWeaponFireModeComponent.h"
#include "SuspenseCore/Components/SuspenseEquipmentMeshComponent.h"
#include "SuspenseCore/Components/SuspenseEquipmentAttributeComponent.h"
#include "ItemSystem/SuspenseItemManager.h"
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

ASuspenseWeaponActor::ASuspenseWeaponActor()
{
    PrimaryActorTick.bCanEverTick = false;

    AmmoComponent     = CreateDefaultSubobject<USuspenseWeaponAmmoComponent>(TEXT("AmmoComponent"));
    FireModeComponent = CreateDefaultSubobject<USuspenseWeaponFireModeComponent>(TEXT("FireModeComponent"));
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

void ASuspenseWeaponActor::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogWeaponActor, Verbose, TEXT("WeaponActor BeginPlay: %s"), *GetName());
}

void ASuspenseWeaponActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Persist ammo / firemode (component already persists on changes; here — final guard)
    SaveWeaponState();

    // Soft cleanup components (no GA/GE touch)
    if (AmmoComponent)     { AmmoComponent->Cleanup(); }
    if (FireModeComponent) { FireModeComponent->Cleanup(); }

    Super::EndPlay(EndPlayReason);
}

USuspenseEventManager* ASuspenseWeaponActor::GetDelegateManager() const
{
    // У этого фасада собственного менеджера нет — события маршрутизируются компонентами/сервисами.
    return nullptr;
}

//================================================
// ASuspenseEquipmentActor override: extend item-equip path
//================================================
void ASuspenseWeaponActor::OnItemInstanceEquipped_Implementation(const FSuspenseInventoryItemInstance& ItemInstance)
{
    // Base: caches ASC, initializes Mesh/Attribute/Attachment from SSOT + fires UI.Equipment.DataReady
    Super::OnItemInstanceEquipped_Implementation(ItemInstance);

    // Load SSOT data for weapon specifics
    if (USuspenseItemManager* ItemManager = GetItemManager())
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
// ISuspenseWeapon (facade)
//================================================
FWeaponInitializationResult ASuspenseWeaponActor::InitializeFromItemData_Implementation(const FSuspenseInventoryItemInstance& ItemInstance)
{
    FWeaponInitializationResult R;

    // Use the same unified path as equip (no GA/GE/Attach here)
    OnItemInstanceEquipped_Implementation(ItemInstance);

    R.bSuccess         = bHasCachedData;
    R.FireModesLoaded  = FireModeComponent ? ISuspenseFireModeProvider::Execute_GetAvailableFireModeCount(FireModeComponent) : 0;
    R.AbilitiesGranted = 0; // actor grants nothing

    if (!R.bSuccess)
    {
        R.ErrorMessage = FText::FromString(TEXT("Failed to initialize weapon from SSOT"));
    }
    return R;
}

bool ASuspenseWeaponActor::GetWeaponItemData_Implementation(FSuspenseUnifiedItemData& OutData) const
{
    if (bHasCachedData)
    {
        OutData = CachedItemData;
        return true;
    }
    return false;
}

FSuspenseInventoryItemInstance ASuspenseWeaponActor::GetItemInstance_Implementation() const
{
    return EquippedItemInstance;
}

bool ASuspenseWeaponActor::Fire_Implementation(const FWeaponFireParams& /*Params*/)
{
    // Actor doesn't simulate fire; ability flow does it.
    return AmmoComponent ? AmmoComponent->ConsumeAmmo(1.0f) : false;
}

void ASuspenseWeaponActor::StopFire_Implementation()
{
    // Intentionally empty (handled by abilities / components)
}

bool ASuspenseWeaponActor::Reload_Implementation(bool bForce)
{
    return AmmoComponent ? AmmoComponent->StartReload(bForce) : false;
}

void ASuspenseWeaponActor::CancelReload_Implementation()
{
    if (AmmoComponent) { AmmoComponent->CancelReload(); }
}

FGameplayTag ASuspenseWeaponActor::GetWeaponArchetype_Implementation() const
{
    return bHasCachedData ? CachedItemData.WeaponArchetype : FGameplayTag::EmptyTag;
}

FGameplayTag ASuspenseWeaponActor::GetWeaponType_Implementation() const
{
    return bHasCachedData ? CachedItemData.ItemType : FGameplayTag::EmptyTag;
}

FGameplayTag ASuspenseWeaponActor::GetAmmoType_Implementation() const
{
    return bHasCachedData ? CachedItemData.AmmoType : FGameplayTag::EmptyTag;
}

FName ASuspenseWeaponActor::GetMuzzleSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.MuzzleSocket : NAME_None;
}

FName ASuspenseWeaponActor::GetSightSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.SightSocket : NAME_None;
}

FName ASuspenseWeaponActor::GetMagazineSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.MagazineSocket : NAME_None;
}

FName ASuspenseWeaponActor::GetGripSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.GripSocket : NAME_None;
}

FName ASuspenseWeaponActor::GetStockSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.StockSocket : NAME_None;
}

float ASuspenseWeaponActor::GetWeaponDamage_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("Damage"), WeaponDefaults::DefaultDamage);
}

float ASuspenseWeaponActor::GetFireRate_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("FireRate"), WeaponDefaults::DefaultFireRate);
}

float ASuspenseWeaponActor::GetReloadTime_Implementation() const
{
    // Delegated to AmmoComponent where possible
    return AmmoComponent ? AmmoComponent->GetReloadTime(/*bTactical*/ true) : WeaponDefaults::DefaultReloadTime;
}

float ASuspenseWeaponActor::GetRecoil_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("Recoil"), WeaponDefaults::DefaultRecoil);
}

float ASuspenseWeaponActor::GetRange_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("Range"), WeaponDefaults::DefaultRange);
}

float ASuspenseWeaponActor::GetBaseSpread_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("BaseSpread"), 0.0f);
}

float ASuspenseWeaponActor::GetMaxSpread_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("MaxSpread"), 0.0f);
}

float ASuspenseWeaponActor::GetCurrentSpread_Implementation() const
{
    // Actor does not simulate dynamic spread anymore; return base value for UI if needed.
    return GetBaseSpread_Implementation();
}

void ASuspenseWeaponActor::SetCurrentSpread_Implementation(float /*NewSpread*/)
{
    // No-op: spread simulation is handled by abilities/components
}

float ASuspenseWeaponActor::GetCurrentAmmo_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetCurrentAmmo() : 0.0f;
}

float ASuspenseWeaponActor::GetRemainingAmmo_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetRemainingAmmo() : 0.0f;
}

float ASuspenseWeaponActor::GetMagazineSize_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetMagazineSize() : 0.0f;
}

FSuspenseInventoryAmmoState ASuspenseWeaponActor::GetAmmoState_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetAmmoState() : FSuspenseInventoryAmmoState();
}

void ASuspenseWeaponActor::SetAmmoState_Implementation(const FSuspenseInventoryAmmoState& NewState)
{
    // IMPORTANT: actor only persists state to ItemInstance, not pushes into component (to avoid recursion)
    if (!EquippedItemInstance.IsValid())
    {
        return;
    }

    EquippedItemInstance.SetRuntimeProperty(WeaponDefaults::Prop_CurrentAmmo,     NewState.CurrentAmmo);
    EquippedItemInstance.SetRuntimeProperty(WeaponDefaults::Prop_RemainingAmmo,   NewState.RemainingAmmo);
}

bool ASuspenseWeaponActor::CanReload_Implementation() const
{
    return AmmoComponent ? AmmoComponent->CanReload() : false;
}

bool ASuspenseWeaponActor::IsMagazineFull_Implementation() const
{
    return AmmoComponent ? AmmoComponent->IsMagazineFull() : true;
}

FWeaponStateFlags ASuspenseWeaponActor::GetWeaponState_Implementation() const
{
    FWeaponStateFlags Flags;
    if (AmmoComponent)
    {
        Flags.bIsReloading = AmmoComponent->IsReloading();
    }
    // bIsFiring / bIsAiming handled by abilities/components
    return Flags;
}

bool ASuspenseWeaponActor::IsInWeaponState_Implementation(const FWeaponStateFlags& State) const
{
    const FWeaponStateFlags Cur = GetWeaponState_Implementation();
    if (State.bIsReloading && !Cur.bIsReloading) return false;
    if (State.bIsFiring    && !Cur.bIsFiring)    return false;
    if (State.bIsAiming    && !Cur.bIsAiming)    return false;
    return true;
}

void ASuspenseWeaponActor::SetWeaponState_Implementation(const FWeaponStateFlags& NewState, bool bEnabled)
{
    // Let components handle the real state transitions
    if (NewState.bIsReloading && AmmoComponent)
    {
        if (bEnabled)  { AmmoComponent->StartReload(false); }
        else           { AmmoComponent->CancelReload();     }
    }
}

//================================================
// ISuspenseFireModeProvider (proxy -> component)
//================================================
bool ASuspenseWeaponActor::InitializeFromWeaponData_Implementation(const FSuspenseUnifiedItemData& WeaponData)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_InitializeFromWeaponData(FireModeComponent, WeaponData)
        : false;
}

void ASuspenseWeaponActor::ClearFireModes_Implementation()
{
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
    {
        ISuspenseFireModeProvider::Execute_ClearFireModes(FireModeComponent);
    }
}

bool ASuspenseWeaponActor::CycleToNextFireMode_Implementation()
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_CycleToNextFireMode(FireModeComponent)
        : false;
}

bool ASuspenseWeaponActor::CycleToPreviousFireMode_Implementation()
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_CycleToPreviousFireMode(FireModeComponent)
        : false;
}

bool ASuspenseWeaponActor::SetFireMode_Implementation(const FGameplayTag& FireModeTag)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_SetFireMode(FireModeComponent, FireModeTag)
        : false;
}

bool ASuspenseWeaponActor::SetFireModeByIndex_Implementation(int32 Index)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_SetFireModeByIndex(FireModeComponent, Index)
        : false;
}

FGameplayTag ASuspenseWeaponActor::GetCurrentFireMode_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetCurrentFireMode(FireModeComponent)
        : FGameplayTag::EmptyTag;
}

FFireModeRuntimeData ASuspenseWeaponActor::GetCurrentFireModeData_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetCurrentFireModeData(FireModeComponent)
        : FFireModeRuntimeData();
}

bool ASuspenseWeaponActor::IsFireModeAvailable_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_IsFireModeAvailable(FireModeComponent, FireModeTag)
        : false;
}

TArray<FFireModeRuntimeData> ASuspenseWeaponActor::GetAllFireModes_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetAllFireModes(FireModeComponent)
        : TArray<FFireModeRuntimeData>();
}

TArray<FGameplayTag> ASuspenseWeaponActor::GetAvailableFireModes_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetAvailableFireModes(FireModeComponent)
        : TArray<FGameplayTag>();
}

int32 ASuspenseWeaponActor::GetAvailableFireModeCount_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetAvailableFireModeCount(FireModeComponent)
        : 0;
}

bool ASuspenseWeaponActor::SetFireModeEnabled_Implementation(const FGameplayTag& FireModeTag, bool bEnabled)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_SetFireModeEnabled(FireModeComponent, FireModeTag, bEnabled)
        : false;
}

void ASuspenseWeaponActor::SetFireModeBlocked_Implementation(const FGameplayTag& FireModeTag, bool bBlocked)
{
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
    {
        ISuspenseFireModeProvider::Execute_SetFireModeBlocked(FireModeComponent, FireModeTag, bBlocked);
    }
}

bool ASuspenseWeaponActor::IsFireModeBlocked_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_IsFireModeBlocked(FireModeComponent, FireModeTag)
        : false;
}

bool ASuspenseWeaponActor::GetFireModeData_Implementation(const FGameplayTag& FireModeTag, FFireModeRuntimeData& OutData) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetFireModeData(FireModeComponent, FireModeTag, OutData)
        : false;
}

TSubclassOf<UGameplayAbility> ASuspenseWeaponActor::GetFireModeAbility_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetFireModeAbility(FireModeComponent, FireModeTag)
        : nullptr;
}

int32 ASuspenseWeaponActor::GetFireModeInputID_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetFireModeInputID(FireModeComponent, FireModeTag)
        : INDEX_NONE;
}

//================================================
// Utility
//================================================
FVector ASuspenseWeaponActor::GetMuzzleLocation() const
{
    const USuspenseEquipmentMeshComponent* MC = MeshComponent;
    if (MC)
    {
        const FName Socket = ISuspenseWeapon::Execute_GetMuzzleSocketName(const_cast<ASuspenseWeaponActor*>(this));
        if (Socket != NAME_None && MC->DoesSocketExist(Socket))
        {
            return MC->GetSocketLocation(Socket);
        }
    }
    return GetActorLocation();
}

FRotator ASuspenseWeaponActor::GetMuzzleRotation() const
{
    const USuspenseEquipmentMeshComponent* MC = MeshComponent;
    if (MC)
    {
        const FName Socket = ISuspenseWeapon::Execute_GetMuzzleSocketName(const_cast<ASuspenseWeaponActor*>(this));
        if (Socket != NAME_None && MC->DoesSocketExist(Socket))
        {
            return MC->GetSocketRotation(Socket);
        }
    }
    return GetActorRotation();
}

FTransform ASuspenseWeaponActor::GetMuzzleTransform() const
{
    const USuspenseEquipmentMeshComponent* MC = MeshComponent;
    if (MC)
    {
        const FName Socket = ISuspenseWeapon::Execute_GetMuzzleSocketName(const_cast<ASuspenseWeaponActor*>(this));
        if (Socket != NAME_None && MC->DoesSocketExist(Socket))
        {
            return MC->GetSocketTransform(Socket);
        }
    }
    return GetActorTransform();
}

void ASuspenseWeaponActor::SaveWeaponState()
{
    if (!EquippedItemInstance.IsValid())
    {
        return;
    }

    // Persist ammo via interface contract (component already calls this on changes)
    if (AmmoComponent)
    {
        const FSuspenseInventoryAmmoState AS = AmmoComponent->GetAmmoState();
        SetAmmoState_Implementation(AS);
    }

    // Persist fire mode index (for quick restore)
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
    {
        const TArray<FFireModeRuntimeData> All = ISuspenseFireModeProvider::Execute_GetAllFireModes(FireModeComponent);
        const FGameplayTag Cur = ISuspenseFireModeProvider::Execute_GetCurrentFireMode(FireModeComponent);

        const int32 Index = All.IndexOfByPredicate([&](const FFireModeRuntimeData& E){ return E.FireModeTag == Cur; });
        if (Index != INDEX_NONE)
        {
            EquippedItemInstance.SetRuntimeProperty(WeaponDefaults::Prop_CurrentFireMode, static_cast<float>(Index));
        }
    }
}

void ASuspenseWeaponActor::RestoreWeaponState()
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
            FSuspenseInventoryAmmoState S;
            S.CurrentAmmo    = Curr;
            S.RemainingAmmo  = Rem;
            S.AmmoType       = GetAmmoType_Implementation();
            S.bHasAmmoState  = true;
            AmmoComponent->SetAmmoState(S); // component handles broadcast + persist
        }
    }

    // Restore fire mode by saved index
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
    {
        const float SavedIndexF = EquippedItemInstance.GetRuntimeProperty(WeaponDefaults::Prop_CurrentFireMode, -1.0f);
        if (SavedIndexF >= 0.0f)
        {
            ISuspenseFireModeProvider::Execute_SetFireModeByIndex(FireModeComponent, FMath::RoundToInt(SavedIndexF));
        }
    }
}

//================================================
// Internal helpers
//================================================
void ASuspenseWeaponActor::SetupComponentsFromItemData(const FSuspenseUnifiedItemData& ItemData)
{
    // Mesh: use ONLY public interface (InitializeFromItemInstance уже вызван базой в S3)
    if (USuspenseEquipmentMeshComponent* MC = MeshComponent)
    {
        MC->SetupWeaponVisuals(ItemData); // применяет оружейные визуальные настройки
    }

    // Link AttributeComponent to AmmoComponent for attribute access
    if (USuspenseEquipmentAttributeComponent* AC = AttributeComponent)
    {
        if (AmmoComponent)
        {
            AmmoComponent->LinkAttributeComponent(AC);
        }
    }

    // Create weapon interface handle for components
    TScriptInterface<ISuspenseWeapon> SelfIface;
    SelfIface.SetObject(this);
    SelfIface.SetInterface(Cast<ISuspenseWeapon>(this));

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

float ASuspenseWeaponActor::GetWeaponAttributeValue(const FName& AttributeName, float DefaultValue) const
{
    if (const USuspenseEquipmentAttributeComponent* AC = AttributeComponent)
    {
        float V = 0.f;
        if (AC->GetAttributeValue(AttributeName.ToString(), V))
        {
            return V;
        }
    }
    return DefaultValue;
}
