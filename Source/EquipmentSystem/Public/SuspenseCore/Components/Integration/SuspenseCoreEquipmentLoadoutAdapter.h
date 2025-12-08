// Copyright Suspense Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HAL/CriticalSection.h"

#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreLoadoutAdapter.h"
#include "SuspenseCore/Interfaces/Core/ISuspenseCoreLoadout.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentOperations.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreTransactionManager.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "Types/Loadout/SuspenseLoadoutManager.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEventDispatcher.h"
#include "SuspenseCore/Interfaces/Inventory/ISuspenseCoreInventoryBridge.h"
#include "Types/Loadout/SuspenseCoreLoadoutManager.h"

class USuspenseCoreEquipmentOperationService;
class USuspenseCoreEventBus;

struct FEquipmentStateSnapshot;
struct FEquipmentOperationRequest;

#include "SuspenseCoreEquipmentLoadoutAdapter.generated.h"

UENUM(BlueprintType)
enum class ESuspenseCoreLoadoutApplicationStrategy : uint8
{
	Replace = 0,
	Merge,
	Selective,
	Validate
};

USTRUCT(BlueprintType)
struct FSuspenseCoreLoadoutValidationOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Loadout|Validation") bool bCheckCharacterClass    = true;
	UPROPERTY(EditAnywhere, Category="Loadout|Validation") bool bCheckInventorySpace   = true;
	UPROPERTY(EditAnywhere, Category="Loadout|Validation") bool bCheckItemAvailability = true;
	UPROPERTY(EditAnywhere, Category="Loadout|Validation") bool bCheckSlotCompatibility= true;
	UPROPERTY(EditAnywhere, Category="Loadout|Validation") bool bCheckWeightLimits     = true;
};

/**
 * Adapter-компонент для применения/сохранения лоадаутов.
 * Поддерживает 2 пути:
 *  - Централизованный OperationService (S8: Adapter → OperationService → ValidationService → DataService → EventBus)
 *  - Фолбэк через TransactionManager/OperationsExecutor
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentLoadoutAdapter : public UActorComponent, public ISuspenseCoreLoadoutAdapter
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentLoadoutAdapter();

	// UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ISuspenseCoreLoadoutAdapter
	virtual FSuspenseCoreLoadoutApplicationResult ApplyLoadout(const FName& LoadoutId, bool bForce=false) override;
	virtual FSuspenseCoreLoadoutApplicationResult ApplyLoadoutWithStrategy(const FName& LoadoutId, ESuspenseCoreLoadoutStrategy Strategy) override;
	virtual FSuspenseCoreLoadoutApplicationResult ApplyLoadoutConfiguration(const FSuspenseCoreLoadoutConfiguration& Configuration, ESuspenseCoreLoadoutStrategy Strategy = ESuspenseCoreLoadoutStrategy::Replace) override;
	virtual bool SaveAsLoadout(const FName& LoadoutId) override;
	virtual bool SaveAsLoadoutWithName(const FName& LoadoutId, const FText& DisplayName) override;
	virtual bool ValidateLoadout(const FName& LoadoutId, TArray<FText>& OutErrors) const override;
	virtual bool ValidateLoadoutWithOptions(const FName& LoadoutId, const FSuspenseCoreLoadoutAdapterOptions& Options, TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const override;
	virtual FName GetCurrentLoadout() const override;
	virtual bool GetLoadoutConfiguration(const FName& LoadoutId, FSuspenseCoreLoadoutConfiguration& OutConfiguration) const override;
	virtual TArray<FName> GetAvailableLoadouts() const override;
	virtual TArray<FName> GetCompatibleLoadouts() const override;
	virtual FSuspenseCoreLoadoutConfiguration ConvertToLoadoutFormat(const FEquipmentStateSnapshot& State) const override;
	virtual TArray<FEquipmentOperationRequest> ConvertFromLoadoutFormat(const FSuspenseCoreLoadoutConfiguration& Configuration) const override;
	virtual FString GetLoadoutPreview(const FName& LoadoutId) const override;
	virtual float EstimateApplicationTime(const FName& LoadoutId) const override;
	virtual bool GetLoadoutDiff(const FName& LoadoutId, TArray<FName>& OutItemsToAdd, TArray<FName>& OutItemsToRemove) const override;
	virtual USuspenseCoreEventBus* GetEventBus() const override;
	virtual void SetEventBus(USuspenseCoreEventBus* InEventBus) override;
	virtual bool IsApplyingLoadout() const override;
	virtual FSuspenseCoreLoadoutApplicationResult GetLastApplicationResult() const override;
	virtual bool CancelApplication() override;

	// Init / config
	UFUNCTION(BlueprintCallable, Category="Loadout|Initialization")
	bool Initialize(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider,
	                TScriptInterface<ISuspenseCoreEquipmentOperations>   InOperations,
	                TScriptInterface<ISuspenseCoreTransactionManager>    InTransactionManager);

	UFUNCTION(BlueprintCallable, Category="Loadout|Configuration")
	void SetApplicationStrategy(ESuspenseCoreLoadoutApplicationStrategy Strategy);

	UFUNCTION(BlueprintCallable, Category="Loadout|Configuration")
	void SetValidationOptions(const FSuspenseCoreLoadoutValidationOptions& Options);

protected:
	// Построение операций для OperationService
	TArray<FEquipmentOperationRequest> CreateOperationsFromLoadout(const FSuspenseCoreLoadoutConfiguration& Config) const;
	FEquipmentOperationRequest CreateEquipOperation(const FEquipmentSlotConfig& SlotConfig, const FName& ItemId, int32 SlotIndex) const;

	// Валидация
	bool ValidateLoadoutConfiguration(const FSuspenseCoreLoadoutConfiguration& Config, const FSuspenseCoreLoadoutValidationOptions& Options, TArray<FText>& OutErrors) const;
	bool CheckSlotCompatibility(const FEquipmentSlotConfig& SlotConfig, const FName& ItemId) const;
	bool CheckInventorySpace(const FSuspenseCoreLoadoutConfiguration& Config) const;
	bool CheckItemAvailability(const FName& ItemId) const;

	// Вспомогательное
	FSuspenseCoreLoadoutConfiguration BuildLoadoutFromCurrentState(const FName& LoadoutId) const;
	FString GenerateLoadoutPreview(const FSuspenseCoreLoadoutConfiguration& Config) const;
	bool ClearCurrentEquipment();
	int32 ApplyStartingEquipment(const TMap<EEquipmentSlotType, FName>& StartingEquipment);
	void NotifyLoadoutChange(const FName& LoadoutId, bool bSuccess);
	void LogAdapterState() const;

	USuspenseCoreLoadoutManager* GetLoadoutManager() const;
	USuspenseItemManager*    GetItemManager() const;

	// Поиск централизованного OperationService (через ServiceLocator)
	USuspenseCoreEquipmentOperationService* GetOperationService(); // <- больше НЕ const

private:
	// Интеграционные интерфейсы
	UPROPERTY() TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProvider;
	UPROPERTY() TScriptInterface<ISuspenseCoreEquipmentOperations>   OperationsExecutor;
	UPROPERTY() TScriptInterface<ISuspenseCoreTransactionManager>    TransactionManager;

	// Настройки
	UPROPERTY(EditAnywhere, Category="Loadout|Configuration")
	ESuspenseCoreLoadoutApplicationStrategy ApplicationStrategy = ESuspenseCoreLoadoutApplicationStrategy::Replace;

	UPROPERTY(EditAnywhere, Category="Loadout|Configuration")
	FSuspenseCoreLoadoutValidationOptions ValidationOptions;

	// Состояние
	UPROPERTY(VisibleInstanceOnly, Category="Loadout|Status")
	FName CurrentLoadoutId = NAME_None;

	UPROPERTY(VisibleInstanceOnly, Category="Loadout|Status")
	FSuspenseCoreLoadoutApplicationResult LastApplicationResult;

	UPROPERTY(VisibleInstanceOnly, Category="Loadout|Status")
	bool bIsApplying = false;

	UPROPERTY(VisibleInstanceOnly, Category="Loadout|Status")
	bool bIsInitialized = false;

	UPROPERTY(VisibleInstanceOnly, Category="Loadout|Status")
	FGuid ActiveTransactionId;

	// Кэш сабсистем
	mutable TWeakObjectPtr<USuspenseCoreLoadoutManager> CachedLoadoutManager;
	mutable TWeakObjectPtr<USuspenseItemManager>   CachedItemManager;
	mutable float LastCacheTime = 0.0f;
	static constexpr float CacheLifetime = 5.0f;

	// Потокобезопасность
	mutable FCriticalSection AdapterCriticalSection;

	// S8: опция предпочесть централизованный сервис операций
	UPROPERTY(EditAnywhere, Category="Loadout|Configuration")
	bool bPreferOperationService = true;

	// Кэш OperationService
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreEquipmentOperationService> CachedOpService;

	// Кэш EventBus
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;
};
