// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Base/SuspenseCoreEquipmentActor.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreFireModeProvider.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreInventoryAmmoState.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "SuspenseCoreWeaponActor.generated.h"

// Forward declarations
class UCameraComponent;
class USuspenseCoreWeaponAmmoComponent;
class USuspenseCoreWeaponFireModeComponent;
class USuspenseCoreEquipmentAttributeComponent;
class USuspenseCoreEventManager;
class USuspenseCoreEquipmentMeshComponent;

/**
 * Thin weapon actor facade (S4).
 * - No GA/GE, no direct Attach or visual hacks.
 * - Initializes components from SSOT and proxies calls to them.
 * - Ammo state persistence happens via component; actor only mirrors to ItemInstance.
 */
UCLASS()
class EQUIPMENTSYSTEM_API ASuspenseCoreWeaponActor : public ASuspenseCoreEquipmentActor,
                                         public ISuspenseCoreWeapon,
                                         public ISuspenseCoreFireModeProvider
{
    GENERATED_BODY()

public:
    ASuspenseCoreWeaponActor();

    //================================================
    // Engine Overrides
    //================================================
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual USuspenseCoreEventManager* GetDelegateManager() const override;

    /**
     * Override CalcCamera to provide correct ADS camera orientation.
     * When SetViewTargetWithBlend points to this weapon, UE calls CalcCamera
     * to get the view parameters. We use the owner's control rotation
     * so the camera looks where the player aims.
     */
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

    //================================================
    // ASuspenseCoreEquipmentActor overrides (S3/S4 pipeline)
    //================================================
    /** Weapon-specific init: extend base item-equip path with weapon setup */
    virtual void OnItemInstanceEquipped_Implementation(const FSuspenseCoreInventoryItemInstance& ItemInstance) override;

    //================================================
    // ISuspenseCoreWeapon (facade -> components)
    //================================================
    virtual FWeaponInitializationResult InitializeFromItemData_Implementation(const FSuspenseCoreInventoryItemInstance& ItemInstance) override;
    virtual bool GetWeaponItemData_Implementation(FSuspenseCoreUnifiedItemData& OutData) const override;
    virtual FSuspenseCoreInventoryItemInstance GetItemInstance_Implementation() const override;

    // Basic actions
    virtual bool Fire_Implementation(const FWeaponFireParams& Params) override;
    virtual void StopFire_Implementation() override;
    virtual bool Reload_Implementation(bool bForce) override;
    virtual void CancelReload_Implementation() override;

    // Identity
    virtual FGameplayTag GetWeaponArchetype_Implementation() const override;
    virtual FGameplayTag GetWeaponType_Implementation() const override;
    virtual FGameplayTag GetAmmoType_Implementation() const override;

    // Sockets (from SSOT)
    virtual FName GetMuzzleSocketName_Implementation() const override;
    virtual FName GetSightSocketName_Implementation() const override;
    virtual FName GetMagazineSocketName_Implementation() const override;
    virtual FName GetGripSocketName_Implementation() const override;
    virtual FName GetStockSocketName_Implementation() const override;

    // Attributes (read-only)
    virtual float GetWeaponDamage_Implementation() const override;
    virtual float GetFireRate_Implementation() const override;
    virtual float GetReloadTime_Implementation() const override;
    virtual float GetRecoil_Implementation() const override;
    virtual float GetRange_Implementation() const override;

    // Spread (actor doesn't simulate it anymore)
    virtual float GetBaseSpread_Implementation() const override;
    virtual float GetMaxSpread_Implementation() const override;
    virtual float GetCurrentSpread_Implementation() const override;
    virtual void  SetCurrentSpread_Implementation(float NewSpread) override;

    // Ammo (delegation to AmmoComponent; SetAmmoState only persists to ItemInstance)
    virtual float GetCurrentAmmo_Implementation() const override;
    virtual float GetRemainingAmmo_Implementation() const override;
    virtual float GetMagazineSize_Implementation() const override;
    virtual FSuspenseCoreInventoryAmmoState GetAmmoState_Implementation() const override;
    virtual void SetAmmoState_Implementation(const FSuspenseCoreInventoryAmmoState& NewState) override;
    virtual bool CanReload_Implementation() const override;
    virtual bool IsMagazineFull_Implementation() const override;

    // Weapon state (flags)
    virtual FWeaponStateFlags GetWeaponState_Implementation() const override;
    virtual bool IsInWeaponState_Implementation(const FWeaponStateFlags& State) const override;
    virtual void SetWeaponState_Implementation(const FWeaponStateFlags& NewState, bool bEnabled) override;

    // ADS Camera Configuration (ISuspenseCoreWeapon)
    virtual float GetADSFieldOfView_Implementation() const override { return AimFOV; }
    virtual float GetADSTransitionDuration_Implementation() const override { return CameraTransitionDuration; }
    virtual bool HasADSScopeCamera_Implementation() const override { return ScopeCam != nullptr; }
    virtual void ResetADSCameraState_Implementation() override { bSmoothedLocationInitialized = false; }

    //================================================
    // ISuspenseCoreFireModeProvider (proxy to component)
    //================================================
    virtual bool InitializeFromWeaponData_Implementation(const FSuspenseCoreUnifiedItemData& WeaponData) override;
    virtual void ClearFireModes_Implementation() override;
    virtual bool CycleToNextFireMode_Implementation() override;
    virtual bool CycleToPreviousFireMode_Implementation() override;
    virtual bool SetFireMode_Implementation(const FGameplayTag& FireModeTag) override;
    virtual bool SetFireModeByIndex_Implementation(int32 Index) override;
    virtual FGameplayTag GetCurrentFireMode_Implementation() const override;
    virtual FSuspenseCoreFireModeRuntimeData GetCurrentFireModeData_Implementation() const override;
    virtual bool IsFireModeAvailable_Implementation(const FGameplayTag& FireModeTag) const override;
    virtual TArray<FSuspenseCoreFireModeRuntimeData> GetAllFireModes_Implementation() const override;
    virtual TArray<FGameplayTag> GetAvailableFireModes_Implementation() const override;
    virtual int32 GetAvailableFireModeCount_Implementation() const override;
    virtual bool SetFireModeEnabled_Implementation(const FGameplayTag& FireModeTag, bool bEnabled) override;
    virtual void SetFireModeBlocked_Implementation(const FGameplayTag& FireModeTag, bool bBlocked) override;
    virtual bool IsFireModeBlocked_Implementation(const FGameplayTag& FireModeTag) const override;
    virtual bool GetFireModeData_Implementation(const FGameplayTag& FireModeTag, FSuspenseCoreFireModeRuntimeData& OutData) const override;
    virtual TSubclassOf<UGameplayAbility> GetFireModeAbility_Implementation(const FGameplayTag& FireModeTag) const override;
    virtual int32 GetFireModeInputID_Implementation(const FGameplayTag& FireModeTag) const override;

    //================================================
    // Utility (no traces/attach here)
    //================================================
    UFUNCTION(BlueprintCallable, Category="Weapon|Geometry")
    FVector GetMuzzleLocation() const;

    UFUNCTION(BlueprintCallable, Category="Weapon|Geometry")
    FRotator GetMuzzleRotation() const;

    UFUNCTION(BlueprintCallable, Category="Weapon|Geometry")
    FTransform GetMuzzleTransform() const;

    // Persist/restore limited runtime state (ammo/firemode index) only
    UFUNCTION(BlueprintCallable, Category="Weapon|State")
    void SaveWeaponState();

    UFUNCTION(BlueprintCallable, Category="Weapon|State")
    void RestoreWeaponState();

    //================================================
    // Animation Pose Getters (for StanceComponent)
    //================================================
    UFUNCTION(BlueprintPure, Category="Weapon|Animation")
    int32 GetGripID() const { return GripID; }

    UFUNCTION(BlueprintPure, Category="Weapon|Animation")
    int32 GetAimPose() const { return AimPose; }

    UFUNCTION(BlueprintPure, Category="Weapon|Animation")
    int32 GetStoredPose() const { return StoredPose; }

    UFUNCTION(BlueprintPure, Category="Weapon|Animation")
    bool GetModifyGrip() const { return bModifyGrip; }

    UFUNCTION(BlueprintPure, Category="Weapon|Animation")
    bool GetCreateAimPose() const { return bCreateAimPose; }

    //================================================
    // ADS Camera API
    //================================================

    /** Get the scope camera for ADS view blending */
    UFUNCTION(BlueprintPure, Category="Weapon|ADS")
    UCameraComponent* GetScopeCamera() const { return ScopeCam; }

    /** Get whether this weapon has a scope camera for ADS */
    UFUNCTION(BlueprintPure, Category="Weapon|ADS")
    bool HasScopeCamera() const { return ScopeCam != nullptr; }

    /** Get the aim field of view for this weapon */
    UFUNCTION(BlueprintPure, Category="Weapon|ADS")
    float GetAimFOV() const { return AimFOV; }

    /** Get the camera transition duration for ADS blend */
    UFUNCTION(BlueprintPure, Category="Weapon|ADS")
    float GetCameraTransitionDuration() const { return CameraTransitionDuration; }

protected:
    /** Setup components from SSOT (ASC is cached by base during equip) */
    void SetupComponentsFromItemData(const FSuspenseCoreUnifiedItemData& ItemData);

    /** Read attribute with fallback default */
    float GetWeaponAttributeValue(const FName& AttributeName, float DefaultValue) const;

    /** Cached weapon data from SSOT */
    UPROPERTY(Transient)
    FSuspenseCoreUnifiedItemData CachedItemData;

    /** Whether CachedItemData is valid */
    UPROPERTY(Transient)
    bool bHasCachedData = false;

    /** Smoothed camera position for ADS (reduces jitter from weapon sway) */
    UPROPERTY(Transient)
    FVector SmoothedCameraLocation = FVector::ZeroVector;

    /** Whether smoothed camera location has been initialized */
    UPROPERTY(Transient)
    bool bSmoothedLocationInitialized = false;

    //================================================
    // Animation Pose Indices (passed to WeaponStanceComponent)
    //================================================

    /** Grip ID - index for left hand grip transform selection from animation DataTable */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Animation|Pose")
    int32 GripID = 0;

    /** Aim Pose index - for Sequence Evaluator in AnimBP */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Animation|Pose")
    int32 AimPose = 1;

    /** Stored Pose index - cached pose for transitions */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Animation|Pose")
    int32 StoredPose = 0;

    /** Whether to modify grip (use DTLHGripTransform instead of LH_Target socket) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Animation|Pose")
    bool bModifyGrip = true;

    /** Whether to create aim pose (use AimPose composite) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Animation|Pose")
    bool bCreateAimPose = true;

    //================================================
    // ADS Camera Configuration
    //================================================

    /** Field of view when aiming down sights (degrees) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Weapon|ADS", meta=(ClampMin="10.0", ClampMax="120.0"))
    float AimFOV = 60.0f;

    /** Duration of camera transition when entering/exiting ADS (seconds) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Weapon|ADS", meta=(ClampMin="0.0", ClampMax="2.0"))
    float CameraTransitionDuration = 0.2f;

    /** Socket name to attach scope camera (typically on weapon's sight) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Weapon|ADS")
    FName ScopeCamSocketName = TEXT("Sight_Socket");

    /**
     * Rotation offset for scope camera relative to socket.
     * Use this to correct camera orientation if socket is rotated differently.
     * Default (0,0,0) means camera uses socket rotation as-is.
     * Example: If camera looks 90° left, set Yaw to -90 to correct.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Weapon|ADS")
    FRotator ScopeCamRotationOffset = FRotator::ZeroRotator;

    /**
     * Location offset for scope camera relative to socket (in socket local space).
     * Use this to fine-tune camera position for proper eye relief and sight alignment.
     * X = Forward/Back along sight line
     * Y = Left/Right
     * Z = Up/Down
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Weapon|ADS")
    FVector ScopeCamLocationOffset = FVector::ZeroVector;

    /**
     * Speed of camera position smoothing during ADS.
     * Higher values = faster tracking (less smoothing), lower = smoother but more lag.
     * Set to 0 to disable smoothing (use raw position).
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Weapon|ADS", meta=(ClampMin="0.0", ClampMax="50.0"))
    float ADSCameraSmoothSpeed = 15.0f;

    //================================================
    // Components (owned by actor)
    //================================================

    /** Scope camera for ADS view blending (optional, attach in Blueprint to Sight_Socket) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    UCameraComponent* ScopeCam;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    USuspenseCoreWeaponAmmoComponent* AmmoComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    USuspenseCoreWeaponFireModeComponent* FireModeComponent;

    //================================================
    // Wall Detection (Weapon Blocking)
    //================================================

    /** Distance threshold for wall detection (units) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Weapon|WallDetection", meta=(ClampMin="10.0", ClampMax="100.0"))
    float WallDetectionDistance = 40.0f;

    /** Interval between wall detection checks (seconds) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Weapon|WallDetection", meta=(ClampMin="0.01", ClampMax="0.5"))
    float WallDetectionInterval = 0.1f;

    /** Enable/disable wall detection for this weapon */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Weapon|WallDetection")
    bool bEnableWallDetection = true;

    /** Timer handle for wall detection */
    FTimerHandle WallDetectionTimerHandle;

    /** Perform wall detection trace and update blocking state */
    void CheckWallBlocking();

    /** Current wall blocking state */
    bool bIsCurrentlyBlocked = false;
};
