// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Base/SuspenseCoreWeaponActor.h"

#include "SuspenseCore/Components/SuspenseCoreWeaponAmmoComponent.h"
#include "SuspenseCore/Components/SuspenseCoreWeaponFireModeComponent.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentMeshComponent.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentAttributeComponent.h"
#include "Camera/CameraComponent.h"
#include "SuspenseCore/ItemSystem/SuspenseCoreItemManager.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreInventoryAmmoState.h"
#include "Engine/SkeletalMeshSocket.h"

// Logging
DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreWeaponActor, Log, All);

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

ASuspenseCoreWeaponActor::ASuspenseCoreWeaponActor()
{
    PrimaryActorTick.bCanEverTick = false;

    AmmoComponent     = CreateDefaultSubobject<USuspenseCoreWeaponAmmoComponent>(TEXT("AmmoComponent"));
    FireModeComponent = CreateDefaultSubobject<USuspenseCoreWeaponFireModeComponent>(TEXT("FireModeComponent"));

    // Create scope camera for ADS view blending
    // Note: Will be attached to Sight_Socket in Blueprint or via SetupComponentsFromItemData
    ScopeCam = CreateDefaultSubobject<UCameraComponent>(TEXT("ScopeCam"));
    ScopeCam->SetupAttachment(RootComponent);
    ScopeCam->bAutoActivate = false;  // Not active by default
    ScopeCam->SetFieldOfView(AimFOV);

    bReplicates = true;
    bNetUseOwnerRelevancy = true;
}

void ASuspenseCoreWeaponActor::BeginPlay()
{
    Super::BeginPlay();

    // Sync ScopeCam FOV with editor-configured AimFOV
    // (constructor runs before editor property changes are applied)
    if (ScopeCam)
    {
        ScopeCam->SetFieldOfView(AimFOV);
    }

    UE_LOG(LogSuspenseCoreWeaponActor, Verbose, TEXT("WeaponActor BeginPlay: %s (AimFOV: %.1f)"), *GetName(), AimFOV);
}

void ASuspenseCoreWeaponActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Persist ammo / firemode (component already persists on changes; here — final guard)
    SaveWeaponState();

    // Soft cleanup components (no GA/GE touch)
    if (AmmoComponent)     { AmmoComponent->Cleanup(); }
    if (FireModeComponent) { FireModeComponent->Cleanup(); }

    Super::EndPlay(EndPlayReason);
}

USuspenseCoreEventManager* ASuspenseCoreWeaponActor::GetDelegateManager() const
{
    // У этого фасада собственного менеджера нет — события маршрутизируются компонентами/сервисами.
    return nullptr;
}

void ASuspenseCoreWeaponActor::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
    // Use ScopeCam position but owner's control rotation
    // This ensures camera is at the sight but looks where player aims

    if (ScopeCam)
    {
        // Get camera location from ScopeCam (attached to Sight_Socket)
        OutResult.Location = ScopeCam->GetComponentLocation();
        OutResult.FOV = ScopeCam->FieldOfView;

        // Get rotation from owner's controller (player's aim direction)
        if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
        {
            if (AController* PC = OwnerPawn->GetController())
            {
                OutResult.Rotation = PC->GetControlRotation();
            }
            else
            {
                // Fallback: use owner pawn's rotation
                OutResult.Rotation = OwnerPawn->GetActorRotation();
            }
        }
        else
        {
            // Fallback: use ScopeCam rotation (may be wrong if socket is misoriented)
            OutResult.Rotation = ScopeCam->GetComponentRotation();
        }

        UE_LOG(LogSuspenseCoreWeaponActor, Verbose,
            TEXT("[CalcCamera] Loc=(%.1f, %.1f, %.1f) Rot=(P=%.1f, Y=%.1f, R=%.1f) FOV=%.1f"),
            OutResult.Location.X, OutResult.Location.Y, OutResult.Location.Z,
            OutResult.Rotation.Pitch, OutResult.Rotation.Yaw, OutResult.Rotation.Roll,
            OutResult.FOV);
    }
    else
    {
        // No scope cam - use default behavior
        Super::CalcCamera(DeltaTime, OutResult);
    }
}

//================================================
// ASuspenseCoreEquipmentActor override: extend item-equip path
//================================================
void ASuspenseCoreWeaponActor::OnItemInstanceEquipped_Implementation(const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    // Base: caches ASC, initializes Mesh/Attribute/Attachment from SSOT + fires UI.Equipment.DataReady
    Super::OnItemInstanceEquipped_Implementation(ItemInstance);

    // Load SSOT data for weapon specifics - try ItemManager first, then DataManager as fallback
    bool bDataLoaded = false;

    // Try ItemManager first
    if (USuspenseCoreItemManager* ItemManager = GetItemManager())
    {
        bDataLoaded = ItemManager->GetUnifiedItemData(ItemInstance.ItemID, CachedItemData);
    }

    // Fallback to DataManager (SSOT) if ItemManager failed
    if (!bDataLoaded)
    {
        UE_LOG(LogSuspenseCoreWeaponActor, Warning, TEXT("ItemManager failed, trying DataManager fallback..."));
        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GI = World->GetGameInstance())
            {
                if (USuspenseCoreDataManager* DataManager = GI->GetSubsystem<USuspenseCoreDataManager>())
                {
                    bDataLoaded = DataManager->GetUnifiedItemData(ItemInstance.ItemID, CachedItemData);
                    if (bDataLoaded)
                    {
                        UE_LOG(LogSuspenseCoreWeaponActor, Warning, TEXT("DataManager fallback succeeded for %s"), *ItemInstance.ItemID.ToString());
                    }
                }
            }
        }
    }

    if (bDataLoaded)
    {
        bHasCachedData = true;

        if (!CachedItemData.bIsWeapon)
        {
            UE_LOG(LogSuspenseCoreWeaponActor, Error, TEXT("Item '%s' is not a weapon in SSOT"), *ItemInstance.ItemID.ToString());
            return;
        }

        // Initialize owned weapon components from SSOT (ONLY public APIs of mesh component)
        SetupComponentsFromItemData(CachedItemData);

        // Restore persisted runtime bits (ammo / fire mode index)
        RestoreWeaponState();

        UE_LOG(LogSuspenseCoreWeaponActor, Log, TEXT("Weapon initialized from SSOT: %s"), *CachedItemData.DisplayName.ToString());
        return;
    }

    UE_LOG(LogSuspenseCoreWeaponActor, Error, TEXT("OnItemInstanceEquipped: failed to read SSOT for ItemID=%s"), *ItemInstance.ItemID.ToString());
}

//================================================
// ISuspenseCoreWeapon (facade)
//================================================
FWeaponInitializationResult ASuspenseCoreWeaponActor::InitializeFromItemData_Implementation(const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    FWeaponInitializationResult R;

    // Use the same unified path as equip (no GA/GE/Attach here)
    OnItemInstanceEquipped_Implementation(ItemInstance);

    R.bSuccess         = bHasCachedData;
    R.FireModesLoaded  = FireModeComponent ? ISuspenseCoreFireModeProvider::Execute_GetAvailableFireModeCount(FireModeComponent) : 0;
    R.AbilitiesGranted = 0; // actor grants nothing

    if (!R.bSuccess)
    {
        R.ErrorMessage = FText::FromString(TEXT("Failed to initialize weapon from SSOT"));
    }
    return R;
}

bool ASuspenseCoreWeaponActor::GetWeaponItemData_Implementation(FSuspenseCoreUnifiedItemData& OutData) const
{
    if (bHasCachedData)
    {
        OutData = CachedItemData;
        return true;
    }
    return false;
}

FSuspenseCoreInventoryItemInstance ASuspenseCoreWeaponActor::GetItemInstance_Implementation() const
{
    return EquippedItemInstance;
}

bool ASuspenseCoreWeaponActor::Fire_Implementation(const FWeaponFireParams& /*Params*/)
{
    // Actor doesn't simulate fire; ability flow does it.
    return AmmoComponent ? AmmoComponent->ConsumeAmmo(1.0f) : false;
}

void ASuspenseCoreWeaponActor::StopFire_Implementation()
{
    // Intentionally empty (handled by abilities / components)
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
    return GetWeaponAttributeValue(TEXT("Damage"), WeaponDefaults::DefaultDamage);
}

float ASuspenseCoreWeaponActor::GetFireRate_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("FireRate"), WeaponDefaults::DefaultFireRate);
}

float ASuspenseCoreWeaponActor::GetReloadTime_Implementation() const
{
    // Delegated to AmmoComponent where possible
    return AmmoComponent ? AmmoComponent->GetReloadTime(/*bTactical*/ true) : WeaponDefaults::DefaultReloadTime;
}

float ASuspenseCoreWeaponActor::GetRecoil_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("Recoil"), WeaponDefaults::DefaultRecoil);
}

float ASuspenseCoreWeaponActor::GetRange_Implementation() const
{
    return GetWeaponAttributeValue(TEXT("Range"), WeaponDefaults::DefaultRange);
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
    // Actor does not simulate dynamic spread anymore; return base value for UI if needed.
    return GetBaseSpread_Implementation();
}

void ASuspenseCoreWeaponActor::SetCurrentSpread_Implementation(float /*NewSpread*/)
{
    // No-op: spread simulation is handled by abilities/components
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

FSuspenseCoreInventoryAmmoState ASuspenseCoreWeaponActor::GetAmmoState_Implementation() const
{
    return AmmoComponent ? AmmoComponent->GetAmmoState() : FSuspenseCoreInventoryAmmoState();
}

void ASuspenseCoreWeaponActor::SetAmmoState_Implementation(const FSuspenseCoreInventoryAmmoState& NewState)
{
    // IMPORTANT: actor only persists state to ItemInstance, not pushes into component (to avoid recursion)
    if (!EquippedItemInstance.IsValid())
    {
        return;
    }

    EquippedItemInstance.SetRuntimeProperty(WeaponDefaults::Prop_CurrentAmmo,     NewState.CurrentAmmo);
    EquippedItemInstance.SetRuntimeProperty(WeaponDefaults::Prop_RemainingAmmo,   NewState.RemainingAmmo);
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
    // bIsFiring / bIsAiming handled by abilities/components
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
    // Let components handle the real state transitions
    if (NewState.bIsReloading && AmmoComponent)
    {
        if (bEnabled)  { AmmoComponent->StartReload(false); }
        else           { AmmoComponent->CancelReload();     }
    }
}

//================================================
// ISuspenseCoreFireModeProvider (proxy -> component)
//================================================
bool ASuspenseCoreWeaponActor::InitializeFromWeaponData_Implementation(const FSuspenseCoreUnifiedItemData& WeaponData)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_InitializeFromWeaponData(FireModeComponent, WeaponData)
        : false;
}

void ASuspenseCoreWeaponActor::ClearFireModes_Implementation()
{
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
    {
        ISuspenseCoreFireModeProvider::Execute_ClearFireModes(FireModeComponent);
    }
}

bool ASuspenseCoreWeaponActor::CycleToNextFireMode_Implementation()
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_CycleToNextFireMode(FireModeComponent)
        : false;
}

bool ASuspenseCoreWeaponActor::CycleToPreviousFireMode_Implementation()
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_CycleToPreviousFireMode(FireModeComponent)
        : false;
}

bool ASuspenseCoreWeaponActor::SetFireMode_Implementation(const FGameplayTag& FireModeTag)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_SetFireMode(FireModeComponent, FireModeTag)
        : false;
}

bool ASuspenseCoreWeaponActor::SetFireModeByIndex_Implementation(int32 Index)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_SetFireModeByIndex(FireModeComponent, Index)
        : false;
}

FGameplayTag ASuspenseCoreWeaponActor::GetCurrentFireMode_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_GetCurrentFireMode(FireModeComponent)
        : FGameplayTag::EmptyTag;
}

FSuspenseCoreFireModeRuntimeData ASuspenseCoreWeaponActor::GetCurrentFireModeData_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_GetCurrentFireModeData(FireModeComponent)
        : FSuspenseCoreFireModeRuntimeData();
}

bool ASuspenseCoreWeaponActor::IsFireModeAvailable_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_IsFireModeAvailable(FireModeComponent, FireModeTag)
        : false;
}

TArray<FSuspenseCoreFireModeRuntimeData> ASuspenseCoreWeaponActor::GetAllFireModes_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_GetAllFireModes(FireModeComponent)
        : TArray<FSuspenseCoreFireModeRuntimeData>();
}

TArray<FGameplayTag> ASuspenseCoreWeaponActor::GetAvailableFireModes_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_GetAvailableFireModes(FireModeComponent)
        : TArray<FGameplayTag>();
}

int32 ASuspenseCoreWeaponActor::GetAvailableFireModeCount_Implementation() const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_GetAvailableFireModeCount(FireModeComponent)
        : 0;
}

bool ASuspenseCoreWeaponActor::SetFireModeEnabled_Implementation(const FGameplayTag& FireModeTag, bool bEnabled)
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_SetFireModeEnabled(FireModeComponent, FireModeTag, bEnabled)
        : false;
}

void ASuspenseCoreWeaponActor::SetFireModeBlocked_Implementation(const FGameplayTag& FireModeTag, bool bBlocked)
{
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
    {
        ISuspenseCoreFireModeProvider::Execute_SetFireModeBlocked(FireModeComponent, FireModeTag, bBlocked);
    }
}

bool ASuspenseCoreWeaponActor::IsFireModeBlocked_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_IsFireModeBlocked(FireModeComponent, FireModeTag)
        : false;
}

bool ASuspenseCoreWeaponActor::GetFireModeData_Implementation(const FGameplayTag& FireModeTag, FSuspenseCoreFireModeRuntimeData& OutData) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_GetFireModeData(FireModeComponent, FireModeTag, OutData)
        : false;
}

TSubclassOf<UGameplayAbility> ASuspenseCoreWeaponActor::GetFireModeAbility_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_GetFireModeAbility(FireModeComponent, FireModeTag)
        : nullptr;
}

int32 ASuspenseCoreWeaponActor::GetFireModeInputID_Implementation(const FGameplayTag& FireModeTag) const
{
    return (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
        ? ISuspenseCoreFireModeProvider::Execute_GetFireModeInputID(FireModeComponent, FireModeTag)
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
        const FName Socket = ISuspenseCoreWeapon::Execute_GetMuzzleSocketName(const_cast<ASuspenseCoreWeaponActor*>(this));
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
        const FName Socket = ISuspenseCoreWeapon::Execute_GetMuzzleSocketName(const_cast<ASuspenseCoreWeaponActor*>(this));
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
        const FName Socket = ISuspenseCoreWeapon::Execute_GetMuzzleSocketName(const_cast<ASuspenseCoreWeaponActor*>(this));
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

    // Persist ammo via interface contract (component already calls this on changes)
    if (AmmoComponent)
    {
        const FSuspenseCoreInventoryAmmoState AS = AmmoComponent->GetAmmoState();
        SetAmmoState_Implementation(AS);
    }

    // Persist fire mode index (for quick restore)
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
    {
        const TArray<FSuspenseCoreFireModeRuntimeData> All = ISuspenseCoreFireModeProvider::Execute_GetAllFireModes(FireModeComponent);
        const FGameplayTag Cur = ISuspenseCoreFireModeProvider::Execute_GetCurrentFireMode(FireModeComponent);

        const int32 Index = All.IndexOfByPredicate([&](const FSuspenseCoreFireModeRuntimeData& E){ return E.FireModeTag == Cur; });
        if (Index != INDEX_NONE)
        {
            EquippedItemInstance.SetRuntimeProperty(WeaponDefaults::Prop_CurrentFireMode, static_cast<float>(Index));
        }
    }
}

void ASuspenseCoreWeaponActor::RestoreWeaponState()
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
            FSuspenseCoreInventoryAmmoState S;
            S.CurrentAmmo    = Curr;
            S.RemainingAmmo  = Rem;
            S.AmmoType       = GetAmmoType_Implementation();
            S.bHasAmmoState  = true;
            AmmoComponent->SetAmmoState(S); // component handles broadcast + persist
        }
    }

    // Restore fire mode by saved index
    if (FireModeComponent && FireModeComponent->GetClass()->ImplementsInterface(USuspenseCoreFireModeProvider::StaticClass()))
    {
        const float SavedIndexF = EquippedItemInstance.GetRuntimeProperty(WeaponDefaults::Prop_CurrentFireMode, -1.0f);
        if (SavedIndexF >= 0.0f)
        {
            ISuspenseCoreFireModeProvider::Execute_SetFireModeByIndex(FireModeComponent, FMath::RoundToInt(SavedIndexF));
        }
    }
}

//================================================
// Internal helpers
//================================================
void ASuspenseCoreWeaponActor::SetupComponentsFromItemData(const FSuspenseCoreUnifiedItemData& ItemData)
{
    UE_LOG(LogSuspenseCoreWeaponActor, Warning,
        TEXT("[ADS Camera Setup] >>> SetupComponentsFromItemData CALLED for %s <<<"), *GetName());

    // Mesh: use ONLY public interface (InitializeFromItemInstance уже вызван базой в S3)
    if (USuspenseCoreEquipmentMeshComponent* MC = MeshComponent)
    {
        UE_LOG(LogSuspenseCoreWeaponActor, Warning,
            TEXT("[ADS Camera Setup] MeshComponent found: %s"), *MC->GetName());

        // Debug: List all available sockets on the mesh
        if (USkeletalMesh* SkelMesh = MC->GetSkeletalMeshAsset())
        {
            UE_LOG(LogSuspenseCoreWeaponActor, Warning,
                TEXT("[ADS Camera Setup] SkeletalMesh: %s"), *SkelMesh->GetName());

            const TArray<USkeletalMeshSocket*>& Sockets = SkelMesh->GetActiveSocketList();
            UE_LOG(LogSuspenseCoreWeaponActor, Warning,
                TEXT("[ADS Camera Setup] Available sockets (%d):"), Sockets.Num());
            for (const USkeletalMeshSocket* Socket : Sockets)
            {
                if (Socket)
                {
                    UE_LOG(LogSuspenseCoreWeaponActor, Warning,
                        TEXT("[ADS Camera Setup]   - '%s' (Bone: %s)"),
                        *Socket->SocketName.ToString(), *Socket->BoneName.ToString());
                }
            }
        }
        else
        {
            UE_LOG(LogSuspenseCoreWeaponActor, Warning,
                TEXT("[ADS Camera Setup] WARNING: No SkeletalMesh asset on MeshComponent!"));
        }

        MC->SetupWeaponVisuals(ItemData); // применяет оружейные визуальные настройки

        // Debug: Check socket existence
        const bool bSocketExists = MC->DoesSocketExist(ScopeCamSocketName);
        UE_LOG(LogSuspenseCoreWeaponActor, Warning,
            TEXT("[ADS Camera Setup] ScopeCam=%s, ScopeCamSocketName='%s', DoesSocketExist=%s"),
            ScopeCam ? TEXT("VALID") : TEXT("NULL"),
            *ScopeCamSocketName.ToString(),
            bSocketExists ? TEXT("TRUE") : TEXT("FALSE"));

        // Attach ScopeCam to Sight_Socket for proper ADS camera orientation
        if (ScopeCam && ScopeCamSocketName != NAME_None && bSocketExists)
        {
            ScopeCam->AttachToComponent(MC, FAttachmentTransformRules::SnapToTargetNotIncludingScale, ScopeCamSocketName);

            // Apply configurable location offset (in socket local space)
            // X = Forward along sight line, Y = Left/Right, Z = Up/Down
            ScopeCam->SetRelativeLocation(ScopeCamLocationOffset);

            // Apply configurable rotation offset
            // If socket orientation doesn't match expected camera direction, adjust via ScopeCamRotationOffset
            ScopeCam->SetRelativeRotation(ScopeCamRotationOffset);

            // Debug logging for camera orientation
            const FTransform SocketTransform = MC->GetSocketTransform(ScopeCamSocketName);
            UE_LOG(LogSuspenseCoreWeaponActor, Warning,
                TEXT("[ADS Camera Setup] SUCCESS! ScopeCam attached to '%s'"), *ScopeCamSocketName.ToString());
            UE_LOG(LogSuspenseCoreWeaponActor, Warning,
                TEXT("[ADS Camera Setup] Socket World Location: X=%.1f Y=%.1f Z=%.1f"),
                SocketTransform.GetLocation().X, SocketTransform.GetLocation().Y, SocketTransform.GetLocation().Z);
            UE_LOG(LogSuspenseCoreWeaponActor, Warning,
                TEXT("[ADS Camera Setup] Socket World Rotation: P=%.1f Y=%.1f R=%.1f"),
                SocketTransform.Rotator().Pitch, SocketTransform.Rotator().Yaw, SocketTransform.Rotator().Roll);
            UE_LOG(LogSuspenseCoreWeaponActor, Warning,
                TEXT("[ADS Camera Setup] ScopeCamLocationOffset: X=%.1f Y=%.1f Z=%.1f"),
                ScopeCamLocationOffset.X, ScopeCamLocationOffset.Y, ScopeCamLocationOffset.Z);
            UE_LOG(LogSuspenseCoreWeaponActor, Warning,
                TEXT("[ADS Camera Setup] ScopeCamRotationOffset: P=%.1f Y=%.1f R=%.1f"),
                ScopeCamRotationOffset.Pitch, ScopeCamRotationOffset.Yaw, ScopeCamRotationOffset.Roll);
            UE_LOG(LogSuspenseCoreWeaponActor, Warning,
                TEXT("[ADS Camera Setup] Final Camera World Location: X=%.1f Y=%.1f Z=%.1f"),
                ScopeCam->GetComponentLocation().X, ScopeCam->GetComponentLocation().Y, ScopeCam->GetComponentLocation().Z);
            UE_LOG(LogSuspenseCoreWeaponActor, Warning,
                TEXT("[ADS Camera Setup] Final Camera World Rotation: P=%.1f Y=%.1f R=%.1f"),
                ScopeCam->GetComponentRotation().Pitch, ScopeCam->GetComponentRotation().Yaw, ScopeCam->GetComponentRotation().Roll);
        }
        else if (ScopeCam)
        {
            UE_LOG(LogSuspenseCoreWeaponActor, Warning,
                TEXT("[ADS Camera Setup] FAILED! ScopeCam exists but socket '%s' not found on mesh!"),
                *ScopeCamSocketName.ToString());
            UE_LOG(LogSuspenseCoreWeaponActor, Warning,
                TEXT("[ADS Camera Setup] Camera will use RootComponent - orientation may be wrong"));
        }
    }
    else
    {
        UE_LOG(LogSuspenseCoreWeaponActor, Warning,
            TEXT("[ADS Camera Setup] ERROR: MeshComponent is NULL!"));
    }

    // Link AttributeComponent to AmmoComponent for attribute access
    if (USuspenseCoreEquipmentAttributeComponent* AC = AttributeComponent)
    {
        if (AmmoComponent)
        {
            AmmoComponent->LinkAttributeComponent(AC);
        }
    }

    // Create weapon interface handle for components
    TScriptInterface<ISuspenseCoreWeapon> SelfIface;
    SelfIface.SetObject(this);
    SelfIface.SetInterface(Cast<ISuspenseCoreWeapon>(this));

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
