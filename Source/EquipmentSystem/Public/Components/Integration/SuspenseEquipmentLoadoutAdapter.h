// Copyright Suspense Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HAL/CriticalSection.h"

#include "Interfaces/Equipment/ISuspenseLoadoutAdapter.h"
#include "Interfaces/Core/ISuspenseLoadout.h"
#include "Types/Loadout/LoadoutSettings.h"

class ISuspenseEquipmentDataProvider;
class ISuspenseEquipmentOperations;
class ISuspenseTransactionManager;
class ISuspenseInventoryBridge;
class ISuspenseEventDispatcher;
class USuspenseLoadoutManager;
class USuspenseItemManager;
class UEquipmentOperationServiceImpl;

struct FEquipmentStateSnapshot;
struct FEquipmentOperationRequest;

#include "SuspenseEquipmentLoadoutAdapter.generated.h"

UENUM(BlueprintType)
enum class ELoadoutApplicationStrategy : uint8
{
	Replace = 0,
	Merge,
	Selective,
	Validate
};

USTRUCT(BlueprintType)
struct FLoadoutValidationOptions
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
class EQUIPMENTSYSTEM_API USuspenseEquipmentLoadoutAdapter : public UActorComponent, public ISuspenseLoadoutAdapter
{
	GENERATED_BODY()

public:
	USuspenseEquipmentLoadoutAdapter();

	// UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ISuspenseLoadoutAdapter
	virtual FLoadoutApplicationResult ApplyLoadout(const FName& LoadoutId, bool bForce=false) override;
	virtual bool SaveAsLoadout(const FName& LoadoutId) override;
	virtual bool ValidateLoadout(const FName& LoadoutId, TArray<FText>& OutErrors) const override;
	virtual FName GetCurrentLoadout() const override;
	virtual FLoadoutConfiguration ConvertToLoadoutFormat(const FEquipmentStateSnapshot& State) const override;
	virtual TArray<FEquipmentOperationRequest> ConvertFromLoadoutFormat(const FLoadoutConfiguration& Loadout) const override;
	virtual FString GetLoadoutPreview(const FName& LoadoutId) const override;

	// Init / config
	UFUNCTION(BlueprintCallable, Category="Loadout|Initialization")
	bool Initialize(TScriptInterface<ISuspenseEquipmentDataProvider> InDataProvider,
	                TScriptInterface<ISuspenseEquipmentOperations>   InOperations,
	                TScriptInterface<ISuspenseTransactionManager>    InTransactionManager);

	UFUNCTION(BlueprintCallable, Category="Loadout|Configuration")
	void SetApplicationStrategy(ELoadoutApplicationStrategy Strategy);

	UFUNCTION(BlueprintCallable, Category="Loadout|Configuration")
	void SetValidationOptions(const FLoadoutValidationOptions& Options);

	UFUNCTION(BlueprintCallable, Category="Loadout|Configuration")
	void SetInventoryBridge(TScriptInterface<ISuspenseInventoryBridge> Bridge);

	UFUNCTION(BlueprintCallable, Category="Loadout|Configuration")
	void SetEventDispatcher(TScriptInterface<ISuspenseEventDispatcher> Dispatcher);

	// Status
	UFUNCTION(BlueprintCallable, Category="Loadout|Status", BlueprintPure)
	FLoadoutApplicationResult GetLastApplicationResult() const { return LastApplicationResult; }

	UFUNCTION(BlueprintCallable, Category="Loadout|Status", BlueprintPure)
	bool IsApplyingLoadout() const { return bIsApplying; }

	// Query
	UFUNCTION(BlueprintCallable, Category="Loadout|Query")
	TArray<FName> GetCompatibleLoadouts() const;

	UFUNCTION(BlueprintCallable, Category="Loadout|Query")
	float EstimateApplicationTime(const FName& LoadoutId) const;

protected:
	// Основная реализация (фолбэк через транзакции)
	FLoadoutApplicationResult ApplyLoadoutConfiguration(const FLoadoutConfiguration& Config, bool bForce);

	// Построение операций для OperationService
	TArray<FEquipmentOperationRequest> CreateOperationsFromLoadout(const FLoadoutConfiguration& Config) const;
	FEquipmentOperationRequest CreateEquipOperation(const FEquipmentSlotConfig& SlotConfig, const FName& ItemId, int32 SlotIndex) const;

	// Валидация
	bool ValidateLoadoutConfiguration(const FLoadoutConfiguration& Config, const FLoadoutValidationOptions& Options, TArray<FText>& OutErrors) const;
	bool CheckSlotCompatibility(const FEquipmentSlotConfig& SlotConfig, const FName& ItemId) const;
	bool CheckInventorySpace(const FLoadoutConfiguration& Config) const;
	bool CheckItemAvailability(const FName& ItemId) const;

	// Вспомогательное
	FLoadoutConfiguration BuildLoadoutFromCurrentState(const FName& LoadoutId) const;
	FString GenerateLoadoutPreview(const FLoadoutConfiguration& Config) const;
	bool ClearCurrentEquipment();
	int32 ApplyStartingEquipment(const TMap<EEquipmentSlotType, FName>& StartingEquipment);
	void NotifyLoadoutChange(const FName& LoadoutId, bool bSuccess);
	void LogAdapterState() const;

	USuspenseLoadoutManager* GetLoadoutManager() const;
	USuspenseItemManager*    GetItemManager() const;

	// Поиск централизованного OperationService (через ServiceLocator)
	UEquipmentOperationServiceImpl* GetOperationService(); // <- больше НЕ const

private:
	// Интеграционные интерфейсы
	UPROPERTY() TScriptInterface<ISuspenseEquipmentDataProvider> DataProvider;
	UPROPERTY() TScriptInterface<ISuspenseEquipmentOperations>   OperationsExecutor;
	UPROPERTY() TScriptInterface<ISuspenseTransactionManager>    TransactionManager;
	UPROPERTY() TScriptInterface<ISuspenseInventoryBridge>       InventoryBridge;
	UPROPERTY() TScriptInterface<ISuspenseEventDispatcher>       EventDispatcher;

	// Настройки
	UPROPERTY(EditAnywhere, Category="Loadout|Configuration")
	ELoadoutApplicationStrategy ApplicationStrategy = ELoadoutApplicationStrategy::Replace;

	UPROPERTY(EditAnywhere, Category="Loadout|Configuration")
	FLoadoutValidationOptions ValidationOptions;

	// Состояние
	UPROPERTY(VisibleInstanceOnly, Category="Loadout|Status")
	FName CurrentLoadoutId = NAME_None;

	UPROPERTY(VisibleInstanceOnly, Category="Loadout|Status")
	FLoadoutApplicationResult LastApplicationResult;

	UPROPERTY(VisibleInstanceOnly, Category="Loadout|Status")
	bool bIsApplying = false;

	UPROPERTY(VisibleInstanceOnly, Category="Loadout|Status")
	bool bIsInitialized = false;

	UPROPERTY(VisibleInstanceOnly, Category="Loadout|Status")
	FGuid ActiveTransactionId;

	// Кэш сабсистем
	mutable TWeakObjectPtr<USuspenseLoadoutManager> CachedLoadoutManager;
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
	TWeakObjectPtr<UEquipmentOperationServiceImpl> CachedOpService;
};
