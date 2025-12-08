// SuspenseCoreServiceLocator.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreServiceLocator.generated.h"

/**
 * USuspenseCoreServiceLocator
 *
 * Простой Service Locator для Dependency Injection.
 * Позволяет регистрировать и получать сервисы по типу.
 *
 * Пример использования:
 *   ServiceLocator->RegisterService<USomeService>(MyService);
 *   auto* Service = ServiceLocator->GetService<USomeService>();
 *
 * Это НОВЫЙ класс, написанный С НУЛЯ.
 * Legacy: SuspenseEquipmentServiceLocator (не трогаем)
 */
UCLASS(BlueprintType)
class BRIDGESYSTEM_API USuspenseCoreServiceLocator : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreServiceLocator();

	// ═══════════════════════════════════════════════════════════════════════════
	// РЕГИСТРАЦИЯ СЕРВИСОВ
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Регистрирует сервис по имени класса (Blueprint).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
	void RegisterServiceByName(FName ServiceName, UObject* ServiceInstance);

	/**
	 * Регистрирует сервис (C++ template версия).
	 */
	template<typename T>
	void RegisterService(T* ServiceInstance)
	{
		static_assert(TIsDerivedFrom<T, UObject>::Value, "Service must derive from UObject");
		RegisterServiceByName(T::StaticClass()->GetFName(), ServiceInstance);
	}

	/**
	 * Регистрирует сервис как интерфейс.
	 */
	template<typename InterfaceType>
	void RegisterServiceAs(FName InterfaceName, UObject* ServiceInstance)
	{
		RegisterServiceByName(InterfaceName, ServiceInstance);
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// ПОЛУЧЕНИЕ СЕРВИСОВ
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Получает сервис по имени (Blueprint).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
	UObject* GetServiceByName(FName ServiceName) const;

	/**
	 * Получает сервис (C++ template версия).
	 */
	template<typename T>
	T* GetService() const
	{
		return Cast<T>(GetServiceByName(T::StaticClass()->GetFName()));
	}

	/**
	 * Получает сервис по имени интерфейса.
	 */
	template<typename T>
	T* GetServiceAs(FName InterfaceName) const
	{
		return Cast<T>(GetServiceByName(InterfaceName));
	}

	/**
	 * Проверяет наличие сервиса.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
	bool HasService(FName ServiceName) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// УПРАВЛЕНИЕ
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Удаляет сервис.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
	void UnregisterService(FName ServiceName);

	/**
	 * Очищает все сервисы.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
	void ClearAllServices();

	/**
	 * Получает список зарегистрированных сервисов.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services|Debug")
	TArray<FName> GetRegisteredServiceNames() const;

	/**
	 * Количество зарегистрированных сервисов.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services|Debug")
	int32 GetServiceCount() const;

protected:
	/** Карта сервисов: ServiceName -> Instance */
	UPROPERTY()
	TMap<FName, TObjectPtr<UObject>> Services;

	/** Критическая секция для thread-safety */
	mutable FCriticalSection ServiceLock;
};

