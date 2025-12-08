// SuspenseItemDataTable.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "AttributeSet.h"
#include "GameplayAbilities/Public/Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "SuspenseItemDataTable.generated.h"

// Forward declarations
class UTexture2D;
class UStaticMesh;
class UNiagaraSystem;
class USoundBase;
class UGameplayAbility;
class UGameplayEffect;

/**
 * Pickup data structure for spawning items in world
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FMCPickupData
{
    GENERATED_BODY()
    
    /** Item identifier */
    UPROPERTY(BlueprintReadOnly)
    FName ItemID = NAME_None;
    
    /** Display name */
    UPROPERTY(BlueprintReadOnly)
    FText ItemName;
    
    /** Item type tag */
    UPROPERTY(BlueprintReadOnly)
    FGameplayTag ItemType;
    
    /** Quantity in pickup */
    UPROPERTY(BlueprintReadOnly)
    int32 Quantity = 1;
    
    /** World mesh reference */
    UPROPERTY(BlueprintReadOnly)
    TSoftObjectPtr<UStaticMesh> WorldMesh;
    
    /** Spawn VFX */
    UPROPERTY(BlueprintReadOnly)
    TSoftObjectPtr<UNiagaraSystem> SpawnEffect;
    
    /** Pickup sound */
    UPROPERTY(BlueprintReadOnly)
    TSoftObjectPtr<USoundBase> PickupSound;
};

/**
 * Equipment data structure for equipped items
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FMCEquipmentData
{
    GENERATED_BODY()
    
    /** Item identifier */
    UPROPERTY(BlueprintReadOnly)
    FName ItemID = NAME_None;
    
    /** Equipment slot type */
    UPROPERTY(BlueprintReadOnly)
    FGameplayTag SlotType;
    
    /** Actor class to spawn */
    UPROPERTY(BlueprintReadOnly)
    TSoftClassPtr<AActor> ActorClass;
    
    /** Active attachment socket */
    UPROPERTY(BlueprintReadOnly)
    FName AttachmentSocket = NAME_None;
    
    /** Inactive storage socket */
    UPROPERTY(BlueprintReadOnly)
    FName UnequippedSocket = NAME_None;
    
    /** Active attachment transform offset */
    UPROPERTY(BlueprintReadOnly)
    FTransform AttachmentOffset = FTransform::Identity;
    
    /** Inactive attachment transform offset */
    UPROPERTY(BlueprintReadOnly)
    FTransform UnequippedOffset = FTransform::Identity;
    
    /** AttributeSet class for equipment stats */
    UPROPERTY(BlueprintReadOnly)
    TSubclassOf<UAttributeSet> AttributeSetClass;
    
    /** Initialization effect for attributes */
    UPROPERTY(BlueprintReadOnly)
    TSubclassOf<UGameplayEffect> InitializationEffect;
    
    /** Gameplay abilities granted by equipment */
    UPROPERTY(BlueprintReadOnly)
    TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;
};

/**
 * Weapon initialization data for GAS integration
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FWeaponInitializationData
{
    GENERATED_BODY()
    
    /** AttributeSet класс для характеристик оружия */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initialization", 
        meta = (MustImplement = "/Script/GameplayAbilities.AttributeSet"))
    TSubclassOf<UAttributeSet> WeaponAttributeSetClass;
    
    /** GameplayEffect для инициализации атрибутов оружия */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initialization")
    TSubclassOf<UGameplayEffect> WeaponInitEffect;
    
    FWeaponInitializationData()
    {
        WeaponAttributeSetClass = nullptr;
        WeaponInitEffect = nullptr;
    }
};

/**
 * Ammo initialization data for GAS integration
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FAmmoInitializationData
{
    GENERATED_BODY()
    
    /** AttributeSet класс для характеристик патронов */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initialization", 
        meta = (MustImplement = "/Script/GameplayAbilities.AttributeSet"))
    TSubclassOf<UAttributeSet> AmmoAttributeSetClass;
    
    /** GameplayEffect для инициализации атрибутов патронов */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initialization")
    TSubclassOf<UGameplayEffect> AmmoInitEffect;
    
    FAmmoInitializationData()
    {
        AmmoAttributeSetClass = nullptr;
        AmmoInitEffect = nullptr;
    }
};

/**
 * Armor initialization data for GAS integration
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FArmorInitializationData
{
    GENERATED_BODY()
    
    /** AttributeSet класс для характеристик брони */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initialization", 
        meta = (MustImplement = "/Script/GameplayAbilities.AttributeSet"))
    TSubclassOf<UAttributeSet> ArmorAttributeSetClass;
    
    /** GameplayEffect для инициализации атрибутов брони */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initialization")
    TSubclassOf<UGameplayEffect> ArmorInitEffect;
    
    FArmorInitializationData()
    {
        ArmorAttributeSetClass = nullptr;
        ArmorInitEffect = nullptr;
    }
};

/**
 * General equipment initialization data
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FEquipmentInitializationData
{
    GENERATED_BODY()
    
    /** AttributeSet класс для характеристик экипировки */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initialization", 
        meta = (MustImplement = "/Script/GameplayAbilities.AttributeSet"))
    TSubclassOf<UAttributeSet> EquipmentAttributeSetClass;
    
    /** GameplayEffect для инициализации */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initialization")
    TSubclassOf<UGameplayEffect> EquipmentInitEffect;
    
    FEquipmentInitializationData()
    {
        EquipmentAttributeSetClass = nullptr;
        EquipmentInitEffect = nullptr;
    }
};

/**
 * Fire mode configuration for weapons
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FWeaponFireModeData
{
    GENERATED_BODY()
    
    /** Fire mode identifier tag */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireMode")
    FGameplayTag FireModeTag;
    
    /** Display name for UI */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireMode")
    FText DisplayName;
    
    /** GameplayAbility that implements this fire mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireMode")
    TSubclassOf<UGameplayAbility> FireModeAbility;
    
    /** Input binding ID for this fire mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireMode")
    int32 InputID = 0;
    
    /** Is this fire mode enabled by default */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FireMode")
    bool bEnabled = true;
    
    FWeaponFireModeData()
    {
        FireModeTag = FGameplayTag::RequestGameplayTag(FName("Weapon.FireMode.Single"));
        DisplayName = FText::FromString(TEXT("Single"));
    }
};

/**
 * Granted ability configuration with input binding
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FGrantedAbilityData
{
    GENERATED_BODY()
    
    /** Ability class to grant */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    TSubclassOf<UGameplayAbility> AbilityClass;
    
    /** Initial ability level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    int32 AbilityLevel = 1;
    
    /** Input binding tag (optional) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability", meta = (Categories = "Input"))
    FGameplayTag InputTag;
    
    /** Required tags to activate ability */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
    FGameplayTagContainer ActivationRequiredTags;
};

/**
 * Production-ready unified item data structure
 * Complete GAS integration with AttributeSets as single source of truth
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseUnifiedItemData : public FTableRowBase
{
    GENERATED_BODY()
    
    //==================================================================
    // Core Identity
    //==================================================================
    
    /** Unique item identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Identity")
    FName ItemID = NAME_None;
    
    /** Localized display name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Identity")
    FText DisplayName;
    
    /** Localized description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Identity", meta = (MultiLine = "true"))
    FText Description;
    
    /** Item icon for UI */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Identity")
    TSoftObjectPtr<UTexture2D> Icon;
    
    //==================================================================
    // Type Classification
    //==================================================================
    
    /** Primary item type tag */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Classification", meta = (Categories = "Item"))
    FGameplayTag ItemType;
    
    /** Item rarity tag */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Classification", meta = (Categories = "Item.Rarity"))
    FGameplayTag Rarity;
    
    /** Additional classification tags */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Classification", meta = (Categories = "Item"))
    FGameplayTagContainer ItemTags;
    
    //==================================================================
    // Inventory Properties
    //==================================================================
    
    /** Size in inventory grid */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Inventory", meta = (ClampMin = "1", ClampMax = "10"))
    FIntPoint GridSize = FIntPoint(1, 1);
    
    /** Maximum stack size (1 = non-stackable) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Inventory", meta = (ClampMin = "1", ClampMax = "999"))
    int32 MaxStackSize = 1;
    
    /** Base weight per item (kg) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Inventory", meta = (ClampMin = "0.0"))
    float Weight = 1.0f;
    
    /** Base monetary value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Inventory", meta = (ClampMin = "0"))
    int32 BaseValue = 0;
    
    //==================================================================
    // Usage Configuration
    //==================================================================
    
    /** Can be equipped */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Usage")
    bool bIsEquippable = false;
    
    /** Equipment slot type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Usage", meta = (EditCondition = "bIsEquippable", Categories = "Equipment.Slot"))
    FGameplayTag EquipmentSlot;
    
    /** Can be consumed/used */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Usage")
    bool bIsConsumable = false;
    
    /** Consumption time in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Usage", meta = (EditCondition = "bIsConsumable", ClampMin = "0.0"))
    float UseTime = 1.0f;
    
    /** Can be dropped */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Usage")
    bool bCanDrop = true;
    
    /** Can be traded between players */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Usage")
    bool bCanTrade = true;
    
    /** Quest/mission critical item */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Usage")
    bool bIsQuestItem = false;
    
    //==================================================================
    // Visual Assets
    //==================================================================
    
    /** Mesh for world representation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Visuals")
    TSoftObjectPtr<UStaticMesh> WorldMesh;
    
    /** VFX for pickup spawn */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Visuals")
    TSoftObjectPtr<UNiagaraSystem> PickupSpawnVFX;
    
    /** VFX for pickup collection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Visuals")
    TSoftObjectPtr<UNiagaraSystem> PickupCollectVFX;
    
    /** VFX for item use/consumption */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Visuals", meta = (EditCondition = "bIsConsumable"))
    TSoftObjectPtr<UNiagaraSystem> UseVFX;
    
    //==================================================================
    // Audio Assets
    //==================================================================
    
    /** Sound for pickup */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Audio")
    TSoftObjectPtr<USoundBase> PickupSound;
    
    /** Sound for dropping */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Audio")
    TSoftObjectPtr<USoundBase> DropSound;
    
    /** Sound for use/equip */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Audio", meta = (EditCondition = "bIsConsumable || bIsEquippable"))
    TSoftObjectPtr<USoundBase> UseSound;
    
    //==================================================================
    // Equipment Configuration - Attachment System
    //==================================================================
    
    /** Actor class to spawn when equipped */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Equipment", meta = (EditCondition = "bIsEquippable"))
    TSoftClassPtr<AActor> EquipmentActorClass;
    
    /** 
     * Active attachment socket - where item goes when actively used
     * For weapons: GripPoint (in hands)
     * For armor: actual body socket
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Equipment|Sockets", meta = (EditCondition = "bIsEquippable"))
    FName AttachmentSocket = NAME_None;
    
    /** 
     * Inactive storage socket - where item goes when equipped but not active
     * For primary weapons: WeaponBackSocket (on back)
     * For sidearms: HolsterSocket (on hip)
     * For armor: same as AttachmentSocket (always worn)
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Equipment|Sockets", meta = (EditCondition = "bIsEquippable"))
    FName UnequippedSocket = NAME_None;
    
    /** Transform offset when actively attached */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Equipment|Transform", meta = (EditCondition = "bIsEquippable"))
    FTransform AttachmentOffset = FTransform::Identity;
    
    /** Transform offset when stored/unequipped */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Equipment|Transform", meta = (EditCondition = "bIsEquippable"))
    FTransform UnequippedOffset = FTransform::Identity;
    
    /** AttributeSet for general equipment (not weapon/armor) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Equipment|GAS", meta = (EditCondition = "bIsEquippable && !bIsWeapon && !bIsArmor"))
    TSubclassOf<UAttributeSet> EquipmentAttributeSet;
    
    /** Initialization effect for general equipment */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Equipment|GAS", meta = (EditCondition = "bIsEquippable && !bIsWeapon && !bIsArmor"))
    TSubclassOf<UGameplayEffect> EquipmentInitEffect;
    
    //==================================================================
    // GAS Integration - Equipment
    //==================================================================
    
    /** Abilities granted when equipped */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|GAS", meta = (EditCondition = "bIsEquippable"))
    TArray<FGrantedAbilityData> GrantedAbilities;
    
    /** Passive effects applied when equipped */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|GAS", meta = (EditCondition = "bIsEquippable"))
    TArray<TSubclassOf<UGameplayEffect>> PassiveEffects;
    
    /** Effects applied on consumption */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|GAS", meta = (EditCondition = "bIsConsumable"))
    TArray<TSubclassOf<UGameplayEffect>> ConsumeEffects;
    
    /** Cooldown tag for consumables */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|GAS", meta = (EditCondition = "bIsConsumable", Categories = "Cooldown"))
    FGameplayTag ConsumableCooldownTag;
    
    //==================================================================
    // Weapon Configuration
    //==================================================================
    
    /** Is this item a weapon */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon", meta = (EditCondition = "bIsEquippable"))
    bool bIsWeapon = false;
    
    /** Weapon archetype tag */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon", meta = (EditCondition = "bIsWeapon", Categories = "Weapon.Type"))
    FGameplayTag WeaponArchetype;
    
    /** Compatible ammo type tag */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon", meta = (EditCondition = "bIsWeapon", Categories = "Item.Ammo"))
    FGameplayTag AmmoType;
    
    /** Available fire modes with their abilities */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon", meta = (EditCondition = "bIsWeapon"))
    TArray<FWeaponFireModeData> FireModes;
    
    /** Default fire mode tag */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon", meta = (EditCondition = "bIsWeapon", Categories = "Weapon.FireMode"))
    FGameplayTag DefaultFireMode;
    
    /** Weapon initialization data - contains AttributeSet and effects */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon|GAS", meta = (EditCondition = "bIsWeapon"))
    FWeaponInitializationData WeaponInitialization;
    
    /** AttributeSet for weapon ammo */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon|GAS", 
        meta = (EditCondition = "bIsWeapon", 
                MustImplement = "/Script/GameplayAbilities.AttributeSet"))
    TSubclassOf<UAttributeSet> AmmoAttributeSet;
    
    /** Attachment sockets for weapon modifications */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon|Sockets", meta = (EditCondition = "bIsWeapon"))
    FName MuzzleSocket = "Muzzle";
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon|Sockets", meta = (EditCondition = "bIsWeapon"))
    FName SightSocket = "Sight";
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon|Sockets", meta = (EditCondition = "bIsWeapon"))
    FName MagazineSocket = "Magazine";
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon|Sockets", meta = (EditCondition = "bIsWeapon"))
    FName GripSocket = "Grip";
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon|Sockets", meta = (EditCondition = "bIsWeapon"))
    FName StockSocket = "Stock";
    
    /** Weapon sounds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon|Audio", meta = (EditCondition = "bIsWeapon"))
    TSoftObjectPtr<USoundBase> FireSound;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon|Audio", meta = (EditCondition = "bIsWeapon"))
    TSoftObjectPtr<USoundBase> ReloadSound;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Weapon|Audio", meta = (EditCondition = "bIsWeapon"))
    TSoftObjectPtr<USoundBase> FireModeSwitchSound;
    
    //==================================================================
    // Armor Configuration
    //==================================================================
    
    /** Is this item armor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Armor", meta = (EditCondition = "bIsEquippable && !bIsWeapon"))
    bool bIsArmor = false;
    
    /** Armor type tag */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Armor", meta = (EditCondition = "bIsArmor", Categories = "Armor.Type"))
    FGameplayTag ArmorType;
    
    /** Armor initialization data */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Armor|GAS", meta = (EditCondition = "bIsArmor"))
    FArmorInitializationData ArmorInitialization;
    
    //==================================================================
    // Ammunition Configuration
    //==================================================================
    
    /** Is this item ammunition */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Ammo")
    bool bIsAmmo = false;
    
    /** Caliber/type tag matching weapon's AmmoType */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Ammo", meta = (EditCondition = "bIsAmmo", Categories = "Item.Ammo"))
    FGameplayTag AmmoCaliber;
    
    /** Compatible weapon types */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Ammo", meta = (EditCondition = "bIsAmmo", Categories = "Weapon.Type"))
    FGameplayTagContainer CompatibleWeapons;
    
    /** Ammo initialization data */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Ammo|GAS", meta = (EditCondition = "bIsAmmo"))
    FAmmoInitializationData AmmoInitialization;
    
    /** Quality of ammunition */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Ammo", meta = (EditCondition = "bIsAmmo", Categories = "Item.Ammo.Quality"))
    FGameplayTag AmmoQuality;
    
    /** Special properties tags */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Ammo", meta = (EditCondition = "bIsAmmo", Categories = "Effect.Ammo"))
    FGameplayTagContainer AmmoSpecialProperties;
    
    /** Projectile effects for ranged weapons */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Ammo|Effects", meta = (EditCondition = "bIsAmmo"))
    TArray<TSubclassOf<UGameplayEffect>> ProjectileEffects;
    
    //==================================================================
    // Runtime Configuration
    //==================================================================
    
    /** Pickup actor class */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Runtime")
    TSoftClassPtr<AActor> PickupActorClass;
    
    //==================================================================
    // Constructor
    //==================================================================
    
    FSuspenseUnifiedItemData()
    {
        DisplayName = FText::FromString(TEXT("New Item"));
        Description = FText::FromString(TEXT("Item description"));
        GridSize = FIntPoint(1, 1);
        MaxStackSize = 1;
        Weight = 1.0f;
        bCanDrop = true;
        bCanTrade = true;
        ItemType = FGameplayTag::RequestGameplayTag(TEXT("Item.Generic"));
        Rarity = FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Common"));
    }
    
    //==================================================================
    // Conversion Methods
    //==================================================================
    
    /** Convert to pickup spawn data */
    FMCPickupData ToPickupData(int32 Quantity = 1) const;
    
    /** Convert to equipment data */
    FMCEquipmentData ToEquipmentData() const;
    
    //==================================================================
    // Validation
    //==================================================================
    
    /** Validate data integrity */
    bool IsValid() const;
    
    /** Get validation errors */
    TArray<FText> GetValidationErrors() const;
    
    /** Auto-fix common data issues */
    void SanitizeData();
    
    //==================================================================
    // Helpers
    //==================================================================
    
    /** Get effective item type for categorization */
    FGameplayTag GetEffectiveItemType() const;
    
    /** Check if item matches tag query */
    bool MatchesTags(const FGameplayTagContainer& Tags) const;
    
    /** Get rarity color for UI */
    FLinearColor GetRarityColor() const;
    
    /** Check if this is a ranged weapon */
    bool IsRangedWeapon() const
    {
        return bIsWeapon && WeaponArchetype.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Weapon.Type.Ranged")));
    }
    
    /** Check if this is a melee weapon */
    bool IsMeleeWeapon() const
    {
        return bIsWeapon && WeaponArchetype.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Weapon.Type.Melee")));
    }
    
    /** 
     * Get appropriate socket based on active state
     * @param bIsActive Whether the item is currently active/in use
     * @return Socket name for attachment
     */
    FName GetSocketForState(bool bIsActive) const
    {
        return bIsActive ? AttachmentSocket : UnequippedSocket;
    }
    
    /** 
     * Get appropriate offset based on active state
     * @param bIsActive Whether the item is currently active/in use
     * @return Transform offset for attachment
     */
    FTransform GetOffsetForState(bool bIsActive) const
    {
        return bIsActive ? AttachmentOffset : UnequippedOffset;
    }
    
    //==================================================================
    // AttributeSet Accessors for compatibility
    //==================================================================
    
    /** Get weapon AttributeSet class */
    FORCEINLINE TSubclassOf<UAttributeSet> GetWeaponAttributeSet() const 
    { 
        return bIsWeapon ? WeaponInitialization.WeaponAttributeSetClass : nullptr; 
    }
    
    /** Get armor AttributeSet class */
    FORCEINLINE TSubclassOf<UAttributeSet> GetArmorAttributeSet() const 
    { 
        return bIsArmor ? ArmorInitialization.ArmorAttributeSetClass : nullptr; 
    }
    
    /** Get appropriate AttributeSet for item type */
    TSubclassOf<UAttributeSet> GetPrimaryAttributeSet() const
    {
        if (bIsWeapon) return WeaponInitialization.WeaponAttributeSetClass;
        if (bIsArmor) return ArmorInitialization.ArmorAttributeSetClass;
        return EquipmentAttributeSet;
    }
    
#if WITH_EDITOR
    /** Handle data table changes in editor */
    virtual void OnDataTableChanged(const UDataTable* InDataTable, const FName InRowName) override;
#endif
};