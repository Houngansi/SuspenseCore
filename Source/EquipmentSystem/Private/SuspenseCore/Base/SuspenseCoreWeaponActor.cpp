// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Base/SuspenseCoreWeaponActor.h"

#include "SuspenseCore/Components/SuspenseCoreWeaponAmmoComponent.h"
#include "SuspenseCore/Components/SuspenseCoreWeaponFireModeComponent.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentMeshComponent.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentAttributeComponent.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"

// Logging
DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreWeaponActor, Log, All);

namespace SuspenseCoreWeaponDefaults
{
    static constexpr float DefaultDamage      = 25.0f;
    static constexpr float DefaultFireRate    = 600.0f;
    static constexpr float DefaultReloadTime  = 2.5f;
    static constexpr float DefaultRecoil      = 1.0f;
    static constexpr float DefaultRange       = 10000.0f;

    // Runtime property keys (persist only via ItemInstance)
    static const FName Prop_CurrentAmmo     (TEXT("CurrentAmmo"));
    static const FName Prop_RemainingAmmo   (TEXT("RemainingAmmo"));
    static const FName Prop_CurrentFireMode (TEXT("CurrentFireMode"));
}

ASuspenseCoreWeaponActor::ASuspenseCoreWeaponActor()
{
    PrimaryActorTick.bCanEverTick = false;

    AmmoComponent     = CreateDefaultSubobject<USuspenseCoreWeaponAmmoComponent>(TEXT("AmmoComponent"));
    FireModeComponent = CreateDefaultSubobject<USuspenseCoreWeaponFireModeComponent>(TEXT("FireModeComponent"));
    ScopeCamera       = CreateDefaultSubobject<UCameraComponent>(TEXT("ScopeCamera"));

    if (ScopeCamera)
    {
        ScopeCamera->SetupAttachment(RootComponent.Get());
        ScopeCamera->bAutoActivate = false;
    }

    bReplicates = true;
    bNetUseOwnerRelevancy = true;
}

void ASuspenseCoreWeaponActor::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogSuspenseCoreWeaponActor, Verbose, TEXT("SuspenseCoreWeaponActor BeginPlay: %s"), *GetName());
}

void ASuspenseCoreWeaponActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    SaveWeaponState();

    if (AmmoComponent)     { AmmoComponent->Cleanup(); }
    if (FireModeComponent) { FireModeComponent->Cleanup(); }

    Super::EndPlay(EndPlayReason);
}

USuspenseEventManager* ASuspenseCoreWeaponActor::GetDelegateManager() const
{
    return nullptr;
}

//================================================
// ASuspenseCoreEquipmentActor override
//================================================
void ASuspenseCoreWeaponActor::OnItemInstanceEquipped_Implementation(const FSuspenseInventoryItemInstance& ItemInstance)
{
    Super::OnItemInstanceEquipped_Implementation(ItemInstance);

    if (USuspenseItemManager* ItemManager = GetItemManager())
    {
        if (ItemManager->GetUnifiedItemData(ItemInstance.ItemID, CachedItemData))
        {
            bHasCachedData = true;

            if (!CachedItemData.bIsWeapon)
            {
                UE_LOG(LogSuspenseCoreWeaponActor, Error, TEXT("Item '%s' is not a weapon in SSOT"), *ItemInstance.ItemID.ToString());
                return;
            }

            SetupComponentsFromItemData(CachedItemData);
            RestoreWeaponState();

            UE_LOG(LogSuspenseCoreWeaponActor, Log, TEXT("Weapon initialized from SSOT: %s"), *CachedItemData.DisplayName.ToString());
            return;
        }
    }

    UE_LOG(LogSuspenseCoreWeaponActor, Error, TEXT("OnItemInstanceEquipped: failed to read SSOT for ItemID=%s"), *ItemInstance.ItemID.ToString());
}

//================================================
// ISuspenseWeapon (facade)
//================================================
FWeaponInitializationResult ASuspenseCoreWeaponActor::InitializeFromItemData_Implementation(const FSuspenseInventoryItemInstance& ItemInstance)
{
    FWeaponInitializationResult R;

    OnItemInstanceEquipped_Implementation(ItemInstance);

    R.bSuccess         = bHasCachedData;
    R.FireModesLoaded  = FireModeComponent ? ISuspenseFireModeProvider::Execute_GetAvailableFireModeCount(FireModeComponent) : 0;
    R.AbilitiesGranted = 0;

    if (!R.bSuccess)
    {
        R.ErrorMessage = FText::FromString(TEXT("Failed to initialize weapon from SSOT"));
    }
    return R;
}

bool ASuspenseCoreWeaponActor::GetWeaponItemData_Implementation(FSuspenseUnifiedItemData& OutData) const
{
    if (bHasCachedData)
    {
        OutData = CachedItemData;
        return true;
    }
    return false;
}

FSuspenseInventoryItemInstance ASuspenseCoreWeaponActor::GetItemInstance_Implementation() const
{
    return EquippedItemInstance;
}

bool ASuspenseCoreWeaponActor::Fire_Implementation(const FWeaponFireParams& /*Params*/)
{
    return AmmoComponent ? AmmoComponent->ConsumeAmmo(1.0f) : false;
}

void ASuspenseCoreWeaponActor::StopFire_Implementation()
{
}

bool ASuspenseCoreWeaponActor::Reload_Implementation(bool bForce)
{
    return AmmoComponent ? AmmoComponent->StartReload(bForce) : false;
}

void ASuspenseCoreWeaponActor::CancelReload_Implementation()
{
    if (AmmoComponent) { AmmoComponent->CancelReload(); }
}

FGameplayTag ASuspenseCoreWeaponActor::GetWeaponArchetype_Implementation() const
{
    return bHasCachedData ? CachedItemData.WeaponArchetype : FGameplayTag::EmptyTag;
}

FGameplayTag ASuspenseCoreWeaponActor::GetWeaponType_Implementation() const
{
    return bHasCachedData ? CachedItemData.ItemType : FGameplayTag::EmptyTag;
}

FGameplayTag ASuspenseCoreWeaponActor::GetAmmoType_Implementation() const
{
    return bHasCachedData ? CachedItemData.AmmoType : FGameplayTag::EmptyTag;
}

FName ASuspenseCoreWeaponActor::GetMuzzleSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.MuzzleSocket : NAME_None;
}

FName ASuspenseCoreWeaponActor::GetSightSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.SightSocket : NAME_None;
}

FName ASuspenseCoreWeaponActor::GetMagazineSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.MagazineSocket : NAME_None;
}

FName ASuspenseCoreWeaponActor::GetGripSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.GripSocket : NAME_None;
}

FName ASuspenseCoreWeaponActor::GetStockSocketName_Implementation() const
{
    return bHasCachedData ? CachedItemData.StockSocket : NAME_None;
}

float ASuspenseCoreWeaponActor::GetWeaponDamage_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("Damage"), SuspenseCoreWeaponDefaults::DefaultDamage);
}

float ASuspenseCoreWeaponActor::GetFireRate_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("FireRate"), SuspenseCoreWeaponDefaults::DefaultFireRate);
}

float ASuspenseCoreWeaponActor::GetReloadTime_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetReloadTime(true) : SuspenseCoreWeaponDefaults::DefaultReloadTime;
}

float ASuspenseCoreWeaponActor::GetRecoil_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("Recoil"), SuspenseCoreWeaponDefaults::DefaultRecoil);
}

float ASuspenseCoreWeaponActor::GetRange_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("Range"), SuspenseCoreWeaponDefaults::DefaultRange);
}

float ASuspenseCoreWeaponActor::GetBaseSpread_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("BaseSpread"), 0.0f);
}

float ASuspenseCoreWeaponActor::GetMaxSpread_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("MaxSpread"), 0.0f);
}

float ASuspenseCoreWeaponActor::GetCurrentSpread_Implementation() const
{
    return GetBaseSpread_Implementation();
}

void ASuspenseCoreWeaponActor::SetCurrentSpread_Implementation(float /*NewSpread*/)
{
}

float ASuspenseCoreWeaponActor::GetCurrentAmmo_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetCurrentAmmo() : 0.0f;
}

float ASuspenseCoreWeaponActor::GetRemainingAmmo_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetRemainingAmmo() : 0.0f;
}

float ASuspenseCoreWeaponActor::GetMagazineSize_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetMagazineSize() : 0.0f;
}

FSuspenseInventoryAmmoState ASuspenseCoreWeaponActor::GetAmmoState_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetAmmoState() : FSuspenseInventoryAmmoState();
}

void ASuspenseCoreWeaponActor::SetAmmoState_Implementation(const FSuspenseInventoryAmmoState& NewState)
{
    if (!EquippedItemInstance.IsValid())
    {
        return;
    }

    EquippedItemInstance.SetRuntimeProperty(SuspenseCoreWeaponDefaults::Prop_CurrentAmmo,   NewState.CurrentAmmo);
    EquippedItemInstance.SetRuntimeProperty(SuspenseCoreWeaponDefaults::Prop_RemainingAmmo, NewState.RemainingAmmo);
}

bool ASuspenseCoreWeaponActor::CanReload_Implementation() const
{
    return AmmoComponent ? AmmoComponent->CanReload() : false;
}

bool ASuspenseCoreWeaponActor::IsMagazineFull_Implementation() const
{
    return AmmoComponent ? AmmoComponent->IsMagazineFull() : true;
}

FWeaponStateFlags ASuspenseCoreWeaponActor::GetWeaponState_Implementation() const
{
    FWeaponStateFlags Flags;
    if (AmmoComponent)
    {
        Flags.bIsReloading = AmmoComponent->IsReloading();
    }
    return Flags;
}

bool ASuspenseCoreWeaponActor::IsInWeaponState_Implementation(const FWeaponStateFlags& State) const
{
    const FWeaponStateFlags Cur = GetWeaponState_Implementation();
    if (State.bIsReloading && !Cur.bIsReloading) return false;
    if (State.bIsFiring    && !Cur.bIsFiring)    return false;
    if (State.bIsAiming    && !Cur.bIsAiming)    return false;
    return true;
}

void ASuspenseCoreWeaponActor::SetWeaponState_Implementation(const FWeaponStateFlags& NewState, bool bEnabled)
{
    if (NewState.bIsReloading && AmmoComponent)
    {
        if (bEnabled)  { AmmoComponent->StartReload(false); }
        else           { AmmoComponent->CancelReload();     }
    }
}

//================================================
// ISuspenseFireModeProvider (proxy -> component)
//================================================
bool ASuspenseCoreWeaponActor::InitializeFromWeaponData_Implementation(const FSuspenseUnifiedItemData& WeaponData)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_InitializeFromWeaponData(FireModeComponent, WeaponData)
        : false;
}

void ASuspenseCoreWeaponActor::ClearFireModes_Implementation()
{
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
    {
        ISuspenseFireModeProvider::Execute_ClearFireModes(FireModeComponent);
    }
}

bool ASuspenseCoreWeaponActor::CycleToNextFireMode_Implementation()
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_CycleToNextFireMode(FireModeComponent)
        : false;
}

bool ASuspenseCoreWeaponActor::CycleToPreviousFireMode_Implementation()
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_CycleToPreviousFireMode(FireModeComponent)
        : false;
}

bool ASuspenseCoreWeaponActor::SetFireMode_Implementation(const FGameplayTag& FireModeTag)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_SetFireMode(FireModeComponent, FireModeTag)
        : false;
}

bool ASuspenseCoreWeaponActor::SetFireModeByIndex_Implementation(int32 Index)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_SetFireModeByIndex(FireModeComponent, Index)
        : false;
}

FGameplayTag ASuspenseCoreWeaponActor::GetCurrentFireMode_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetCurrentFireMode(FireModeComponent)
        : FGameplayTag::EmptyTag;
}

FFireModeRuntimeData ASuspenseCoreWeaponActor::GetCurrentFireModeData_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetCurrentFireModeData(FireModeComponent)
        : FFireModeRuntimeData();
}

bool ASuspenseCoreWeaponActor::IsFireModeAvailable_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_IsFireModeAvailable(FireModeComponent, FireModeTag)
        : false;
}

TArray<FFireModeRuntimeData> ASuspenseCoreWeaponActor::GetAllFireModes_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetAllFireModes(FireModeComponent)
        : TArray<FFireModeRuntimeData>();
}

TArray<FGameplayTag> ASuspenseCoreWeaponActor::GetAvailableFireModes_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetAvailableFireModes(FireModeComponent)
        : TArray<FGameplayTag>();
}

int32 ASuspenseCoreWeaponActor::GetAvailableFireModeCount_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetAvailableFireModeCount(FireModeComponent)
        : 0;
}

bool ASuspenseCoreWeaponActor::SetFireModeEnabled_Implementation(const FGameplayTag& FireModeTag, bool bEnabled)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_SetFireModeEnabled(FireModeComponent, FireModeTag, bEnabled)
        : false;
}

void ASuspenseCoreWeaponActor::SetFireModeBlocked_Implementation(const FGameplayTag& FireModeTag, bool bBlocked)
{
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
    {
        ISuspenseFireModeProvider::Execute_SetFireModeBlocked(FireModeComponent, FireModeTag, bBlocked);
    }
}

bool ASuspenseCoreWeaponActor::IsFireModeBlocked_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_IsFireModeBlocked(FireModeComponent, FireModeTag)
        : false;
}

bool ASuspenseCoreWeaponActor::GetFireModeData_Implementation(const FGameplayTag& FireModeTag, FFireModeRuntimeData& OutData) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetFireModeData(FireModeComponent, FireModeTag, OutData)
        : false;
}

TSubclassOf<UGameplayAbility> ASuspenseCoreWeaponActor::GetFireModeAbility_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetFireModeAbility(FireModeComponent, FireModeTag)
        : nullptr;
}

int32 ASuspenseCoreWeaponActor::GetFireModeInputID_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
        ? ISuspenseFireModeProvider::Execute_GetFireModeInputID(FireModeComponent, FireModeTag)
        : INDEX_NONE;
}

//================================================
// Utility
//================================================
FVector ASuspenseCoreWeaponActor::GetMuzzleLocation() const
{
    const USuspenseCoreEquipmentMeshComponent* MC = MeshComponent;
    if (MC)
    {
        const FName Socket = ISuspenseWeapon::Execute_GetMuzzleSocketName(const_cast<ASuspenseCoreWeaponActor*>(this));
        if (Socket != NAME_None && MC->DoesSocketExist(Socket))
        {
            return MC->GetSocketLocation(Socket);
        }
    }
    return GetActorLocation();
}

FRotator ASuspenseCoreWeaponActor::GetMuzzleRotation() const
{
    const USuspenseCoreEquipmentMeshComponent* MC = MeshComponent;
    if (MC)
    {
        const FName Socket = ISuspenseWeapon::Execute_GetMuzzleSocketName(const_cast<ASuspenseCoreWeaponActor*>(this));
        if (Socket != NAME_None && MC->DoesSocketExist(Socket))
        {
            return MC->GetSocketRotation(Socket);
        }
    }
    return GetActorRotation();
}

FTransform ASuspenseCoreWeaponActor::GetMuzzleTransform() const
{
    const USuspenseCoreEquipmentMeshComponent* MC = MeshComponent;
    if (MC)
    {
        const FName Socket = ISuspenseWeapon::Execute_GetMuzzleSocketName(const_cast<ASuspenseCoreWeaponActor*>(this));
        if (Socket != NAME_None && MC->DoesSocketExist(Socket))
        {
            return MC->GetSocketTransform(Socket);
        }
    }
    return GetActorTransform();
}

void ASuspenseCoreWeaponActor::SaveWeaponState()
{
    if (!EquippedItemInstance.IsValid())
    {
        return;
    }

    if (AmmoComponent)
    {
        const FSuspenseInventoryAmmoState AS = AmmoComponent->GetAmmoState();
        SetAmmoState_Implementation(AS);
    }

    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
    {
        const TArray<FFireModeRuntimeData> All = ISuspenseFireModeProvider::Execute_GetAllFireModes(FireModeComponent);
        const FGameplayTag Cur = ISuspenseFireModeProvider::Execute_GetCurrentFireMode(FireModeComponent);

        const int32 Index = All.IndexOfByPredicate([&](const FFireModeRuntimeData& E){ return E.FireModeTag == Cur; });
        if (Index != INDEX_NONE)
        {
            EquippedItemInstance.SetRuntimeProperty(SuspenseCoreWeaponDefaults::Prop_CurrentFireMode, static_cast<float>(Index));
        }
    }
}

void ASuspenseCoreWeaponActor::RestoreWeaponState()
{
    if (!EquippedItemInstance.IsValid())
    {
        return;
    }

    if (AmmoComponent)
    {
        const float Curr  = EquippedItemInstance.GetRuntimeProperty(SuspenseCoreWeaponDefaults::Prop_CurrentAmmo,   -1.0f);
        const float Rem   = EquippedItemInstance.GetRuntimeProperty(SuspenseCoreWeaponDefaults::Prop_RemainingAmmo, -1.0f);

        if (Curr >= 0.0f && Rem >= 0.0f)
        {
            FSuspenseInventoryAmmoState S;
            S.CurrentAmmo    = Curr;
            S.RemainingAmmo  = Rem;
            S.AmmoType       = GetAmmoType_Implementation();
            S.bHasAmmoState  = true;
            AmmoComponent->SetAmmoState(S);
        }
    }

    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseFireModeProvider::StaticClass()))
    {
        const float SavedIndexF = EquippedItemInstance.GetRuntimeProperty(SuspenseCoreWeaponDefaults::Prop_CurrentFireMode, -1.0f);
        if (SavedIndexF >= 0.0f)
        {
            ISuspenseFireModeProvider::Execute_SetFireModeByIndex(FireModeComponent, FMath::RoundToInt(SavedIndexF));
        }
    }
}

//================================================
// Internal helpers
//================================================
void ASuspenseCoreWeaponActor::SetupComponentsFromItemData(const FSuspenseUnifiedItemData& ItemData)
{
    if (USuspenseCoreEquipmentMeshComponent* MC = MeshComponent)
    {
        MC->SetupWeaponVisuals(ItemData);
    }

    if (USuspenseCoreEquipmentAttributeComponent* AC = AttributeComponent)
    {
        if (AmmoComponent)
        {
            AmmoComponent->LinkAttributeComponent(AC);
        }
    }

    TScriptInterface<ISuspenseWeapon> SelfIface;
    SelfIface.SetObject(this);
    SelfIface.SetInterface(Cast<ISuspenseWeapon>(this));

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

float ASuspenseCoreWeaponActor::GetWeaponAttributeValue(const FName& AttributeName, float DefaultValue) const
{
    if (const USuspenseCoreEquipmentAttributeComponent* AC = AttributeComponent)
    {
        float V = 0.f;
        if (AC->GetAttributeValue(AttributeName.ToString(), V))
        {
            return V;
        }
    }
    return DefaultValue;
}
