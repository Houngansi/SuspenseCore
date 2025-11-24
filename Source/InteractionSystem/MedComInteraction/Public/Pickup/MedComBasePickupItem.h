// MedComBasePickupItem.h
// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/Interaction/IMedComPickupInterface.h"
#include "Interfaces/Interaction/IMedComInteractInterface.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "Types/Inventory/InventoryTypes.h"
#include "MedComBasePickupItem.generated.h"

// Forward declarations
class USphereComponent;
class UStaticMeshComponent;
class UMedComItemManager;
class UDataTable;
class UEventDelegateManager;
class UNiagaraComponent;
class UAudioComponent;

/**
 * Структура для хранения пары ключ-значение предустановленного свойства.
 * Используется вместо TMap для поддержки репликации.
 * Эти свойства будут применены к создаваемому экземпляру предмета при подборе.
 */
USTRUCT(BlueprintType)
struct FPresetPropertyPair
{
    GENERATED_BODY()

    /** Имя свойства (например, "Durability", "Ammo", "Charge") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
    FName PropertyName;

    /** Значение свойства */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
    float PropertyValue;

    /** Конструктор по умолчанию */
    FPresetPropertyPair()
        : PropertyName(NAME_None)
        , PropertyValue(0.0f)
    {
    }

    /** Конструктор с параметрами */
    FPresetPropertyPair(FName InName, float InValue)
        : PropertyName(InName)
        , PropertyValue(InValue)
    {
    }

    /** Оператор сравнения для поиска */
    bool operator==(const FPresetPropertyPair& Other) const
    {
        return PropertyName == Other.PropertyName;
    }
};

/**
 * Base pickup actor class
 * Works exclusively with unified DataTable system
 * 
 * АРХИТЕКТУРА:
 * - Единый источник истины: FMedComUnifiedItemData в DataTable
 * - ItemID - единственная ссылка на статические данные
 * - Runtime состояние хранится отдельно (количество, состояние патронов)
 * - Предустановленные свойства применяются к создаваемому экземпляру
 * - Использует TArray для репликации вместо TMap
 */
UCLASS(Blueprintable)
class MEDCOMINTERACTION_API AMedComBasePickupItem : public AActor, 
    public IMedComInteractInterface, 
    public IMedComPickupInterface
{
    GENERATED_BODY()
    
public:
    AMedComBasePickupItem();
    
    //~ Begin AActor Interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void OnConstruction(const FTransform& Transform) override;
    //~ End AActor Interface
    
    //~ Begin IMedComInteractInterface
    virtual bool CanInteract_Implementation(APlayerController* InstigatingController) const override;
    virtual bool Interact_Implementation(APlayerController* InstigatingController) override;
    virtual FGameplayTag GetInteractionType_Implementation() const override;
    virtual FText GetInteractionText_Implementation() const override;
    virtual int32 GetInteractionPriority_Implementation() const override;
    virtual float GetInteractionDistance_Implementation() const override;
    virtual void OnInteractionFocusGained_Implementation(APlayerController* InstigatingController) override;
    virtual void OnInteractionFocusLost_Implementation(APlayerController* InstigatingController) override;
    virtual UEventDelegateManager* GetDelegateManager() const override;
    //~ End IMedComInteractInterface
    
    //~ Begin IMedComPickupInterface
    virtual FName GetItemID_Implementation() const override;
    virtual void SetItemID_Implementation(FName NewItemID) override;
    virtual bool GetUnifiedItemData_Implementation(FMedComUnifiedItemData& OutItemData) const override;
    virtual int32 GetItemAmount_Implementation() const override;
    virtual void SetAmount_Implementation(int32 NewAmount) override;
    virtual bool HasSavedAmmoState_Implementation() const override;
    virtual bool GetSavedAmmoState_Implementation(float& OutCurrentAmmo, float& OutRemainingAmmo) const override;
    virtual void SetSavedAmmoState_Implementation(float CurrentAmmo, float RemainingAmmo) override;
    virtual bool HandlePickedUp_Implementation(AActor* InstigatorActor) override;
    virtual bool CanBePickedUpBy_Implementation(AActor* InstigatorActor) const override;
    virtual FGameplayTag GetItemType_Implementation() const override;
    virtual bool CreateItemInstance_Implementation(FInventoryItemInstance& OutInstance) const override;
    virtual FGameplayTag GetItemRarity_Implementation() const override;
    virtual FText GetDisplayName_Implementation() const override;
    virtual bool IsStackable_Implementation() const override;
    virtual float GetItemWeight_Implementation() const override;
    //~ End IMedComPickupInterface
    
    /** Get mesh component */
    UFUNCTION(BlueprintCallable, Category = "Pickup")
    UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
    
    /** Convenience method to set ammo state */
    UFUNCTION(BlueprintCallable, Category = "Pickup|Weapon")
    void SetAmmoState(bool bHasState, float CurrentAmmo, float RemainingAmmo);
    
    /**
     * Initialize pickup from complete runtime instance
     * Preserves all runtime properties like durability, modifications, etc.
     * @param Instance Runtime item instance with all properties
     */
    UFUNCTION(BlueprintCallable, Category = "Pickup")
    void InitializeFromInstance(const FInventoryItemInstance& Instance);
    
    /**
     * Initialize pickup from spawn data
     * @param SpawnData Spawn configuration with preset properties
     */
    UFUNCTION(BlueprintCallable, Category = "Pickup")
    void InitializeFromSpawnData(const FPickupSpawnData& SpawnData);
    
    //==================================================================
    // Preset Property Management - для свойств, которые будут применены к предмету
    //==================================================================
    
    /**
     * Получает значение предустановленного свойства по имени
     * Эти свойства будут применены к предмету при подборе
     * @param PropertyName Имя свойства для поиска
     * @param DefaultValue Значение по умолчанию, если свойство не найдено
     * @return Значение свойства или DefaultValue
     */
    UFUNCTION(BlueprintCallable, Category = "Pickup|Preset")
    float GetPresetProperty(FName PropertyName, float DefaultValue = 0.0f) const;
    
    /**
     * Устанавливает значение предустановленного свойства
     * Это свойство будет применено к создаваемому экземпляру предмета
     * @param PropertyName Имя свойства
     * @param Value Новое значение
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Pickup|Preset")
    void SetPresetProperty(FName PropertyName, float Value);
    
    /**
     * Проверяет наличие предустановленного свойства
     * @param PropertyName Имя свойства для проверки
     * @return True если свойство существует
     */
    UFUNCTION(BlueprintCallable, Category = "Pickup|Preset")
    bool HasPresetProperty(FName PropertyName) const;
    
    /**
     * Удаляет предустановленное свойство
     * @param PropertyName Имя свойства для удаления
     * @return True если свойство было удалено
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Pickup|Preset")
    bool RemovePresetProperty(FName PropertyName);
    
    /**
     * Получает все предустановленные свойства как TMap для удобства
     * @return Map со всеми свойствами
     */
    UFUNCTION(BlueprintCallable, Category = "Pickup|Preset")
    TMap<FName, float> GetPresetPropertiesAsMap() const;
    
    /**
     * Устанавливает предустановленные свойства из TMap
     * @param NewProperties Map с новыми свойствами
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Pickup|Preset")
    void SetPresetPropertiesFromMap(const TMap<FName, float>& NewProperties);
    
protected:
    //==================================================================
    // Components
    //==================================================================
    
    /** Interaction collision sphere */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* SphereCollision;
    
    /** Visual mesh component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComponent;
    
    /** VFX component for spawn effect */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UNiagaraComponent* SpawnVFXComponent;
    
    /** Audio component for ambient sounds */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UAudioComponent* AudioComponent;
    
    //==================================================================
    // Item Reference - Single Source of Truth
    //==================================================================
    
    /** Item identifier for DataTable lookup */
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Pickup|Item")
    FName ItemID;
    
    /** Quantity of items in this pickup */
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Pickup|Item", meta = (ClampMin = "1"))
    int32 Amount;
    
    //==================================================================
    // Extended Runtime State Support
    //==================================================================
    
 /** 
  * Complete runtime instance data
  * Used when pickup represents dropped equipped item with full state
  * NOTE: Not replicated due to TMap limitation. State is reconstructed from PresetRuntimeProperties
  */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Runtime")
 FInventoryItemInstance RuntimeInstance;
    
    /** 
     * Whether this pickup uses full runtime instance
     * If true, RuntimeInstance is used instead of simple ItemID/Amount
     */
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Pickup|Runtime")
    bool bUseRuntimeInstance = false;
    
    /** 
     * Предустановленные свойства для применения к создаваемому предмету
     * Использует TArray для поддержки репликации
     * Примеры: поврежденное оружие, частично заряженные батареи
     */
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Pickup|Preset", 
        meta = (TitleProperty = "PropertyName"))
    TArray<FPresetPropertyPair> PresetRuntimeProperties;
    
    //==================================================================
    // Weapon State
    //==================================================================
    
    /** Whether this pickup has saved ammo state */
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Pickup|Weapon")
    bool bHasSavedAmmoState;
    
    /** Current ammo in magazine (for dropped weapons) */
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Pickup|Weapon", 
        meta = (EditCondition = "bHasSavedAmmoState"))
    float SavedCurrentAmmo;
    
    /** Remaining ammo (for dropped weapons) */
    UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Pickup|Weapon", 
        meta = (EditCondition = "bHasSavedAmmoState"))
    float SavedRemainingAmmo;
    
    //==================================================================
    // Interaction Settings
    //==================================================================
    
    /** Delay before destroying actor after pickup */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Settings")
    float DestroyDelay;
    
    /** Interaction priority for overlapping pickups */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Settings")
    int32 InteractionPriority;
    
    /** Custom interaction distance (0 = use default) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup|Settings")
    float InteractionDistanceOverride;
    
    //==================================================================
    // Runtime Cache
    //==================================================================
    
    /** Cached unified item data from DataTable */
    UPROPERTY(Transient)
    mutable FMedComUnifiedItemData CachedItemData;
    
    /** Whether item data has been loaded from DataTable */
    UPROPERTY(Transient)
    mutable bool bDataCached;
    
    /** Cached delegate manager */
    mutable TWeakObjectPtr<UEventDelegateManager> CachedDelegateManager;
    
protected:
    //==================================================================
    // Event Handlers
    //==================================================================
    
    /**
     * Called when pickup is successfully collected
     * @param InstigatorActor Actor that collected the pickup
     * @return True if handled successfully
     */
    UFUNCTION(BlueprintNativeEvent, Category = "Pickup")
    bool OnPickedUp(AActor* InstigatorActor);
    virtual bool OnPickedUp_Implementation(AActor* InstigatorActor);
    
    //==================================================================
    // Data Management
    //==================================================================
    
    /**
     * Load item data from DataTable through ItemManager
     * @return True if data was loaded successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Pickup")
    bool LoadItemData() const;
    
    /**
     * Apply visual properties from item data
     */
    UFUNCTION(BlueprintCallable, Category = "Pickup|Visuals")
    virtual void ApplyItemVisuals();
    
    /**
     * Apply audio properties from item data
     */
    UFUNCTION(BlueprintCallable, Category = "Pickup|Audio")
    virtual void ApplyItemAudio();
    
    /**
     * Apply VFX from item data
     */
    UFUNCTION(BlueprintCallable, Category = "Pickup|VFX")
    virtual void ApplyItemVFX();
    
    //==================================================================
    // Blueprint Events
    //==================================================================
    
    /**
     * Blueprint event for custom visual setup
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Pickup|Visuals")
    void OnVisualsApplied();
    
    /**
     * Blueprint event for weapon-specific setup
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Pickup|Weapon")
    void OnWeaponPickupSetup();
    
    /**
     * Blueprint event for armor-specific setup
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "Pickup|Armor")
    void OnArmorPickupSetup();
    
    //==================================================================
    // Utility Methods
    //==================================================================
    
    /**
     * Try to add item to actor's inventory
     * @param InstigatorActor Actor to receive the item
     * @return True if item was added
     */
    UFUNCTION(BlueprintCallable, Category = "Pickup")
    bool TryAddToInventory(AActor* InstigatorActor);
    
    /**
     * Get item manager subsystem
     * @return Item manager or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Pickup")
    UMedComItemManager* GetItemManager() const;
    
    /**
     * Broadcast pickup spawn event
     */
    void BroadcastPickupSpawned();
    
    /**
     * Broadcast pickup collected event
     * @param Collector Actor that collected the pickup
     */
    void BroadcastPickupCollected(AActor* Collector);
    
    /**
     * Handle visual feedback for interaction focus
     * @param bGainedFocus True if gained focus, false if lost
     */
    virtual void HandleInteractionFeedback(bool bGainedFocus);
    
private:
    /**
     * Внутренний метод для поиска предустановленного свойства в массиве
     * @param PropertyName Имя свойства
     * @return Указатель на найденный элемент или nullptr
     */
    FPresetPropertyPair* FindPresetProperty(FName PropertyName);
    const FPresetPropertyPair* FindPresetProperty(FName PropertyName) const;
};