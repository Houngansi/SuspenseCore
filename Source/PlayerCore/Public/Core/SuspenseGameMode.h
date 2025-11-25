// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/SuspenseGameState.h" // Include our custom GameState
#include "SuspenseGameMode.generated.h"

/**
 * Базовый класс GameMode для Suspense
 * Определяет основные правила игры и системные классы
 */
UCLASS()
class SUSPENSECORE_API ASuspenseGameMode : public AGameModeBase
{
    GENERATED_BODY()
    
public:
    // Конструктор с настройкой базовых классов
    ASuspenseGameMode();
    
    // Инициализация
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    
    // Инициализация нового игрока (правильная сигнатура)
    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;
    
    // Старт игры
    virtual void StartPlay() override;
    
    // Завершение игры
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    /**
     * Установка готовности игры к старту
     * @param bIsReady Флаг готовности
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|GameMode")
    void SetReadyToStart(bool bIsReady);
    
    /**
     * Получение статуса готовности игры
     * @return True, если игра готова к старту
     */
    UFUNCTION(BlueprintPure, Category = "MedCom|GameMode")
    bool IsReadyToStart() const;
    
    /**
     * Проверяет, завершена ли игра
     * @return True, если игра завершена
     */
    UFUNCTION(BlueprintPure, Category = "MedCom|GameMode")
    bool IsGameOver() const;
    
    /**
     * Завершает игру с указанием победителя
     * @param Winner Победитель (игрок/команда)
     * @param EndGameReason Причина завершения игры
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|GameMode")
    virtual void FinishGame(AActor* Winner, const FString& EndGameReason);
    
    /**
     * Устанавливает состояние матча в нашем собственном GameState
     * @param NewState Новое состояние матча
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|GameMode")
    virtual void SetGameMatchState(EMedComMatchState NewState);
    
    /**
     * Получает текущее состояние матча из нашего GameState
     * @return Текущее состояние матча
     */
    UFUNCTION(BlueprintPure, Category = "MedCom|GameMode")
    EMedComMatchState GetGameMatchState() const;
    
protected:
    // Выбор начальной локации для игрока
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    
    // Обработчик для события подключения нового игрока
    virtual void HandleNewPlayerConnection(APlayerController* NewPlayer);
    
    // Обработчик для события отключения игрока
    virtual void HandlePlayerDisconnection(APlayerController* DisconnectedPlayer);
    
    // Вызывается при готовности всех игроков к старту
    virtual void OnAllPlayersReady();
    
    // Вызывается при окончании инициализации настроек игры
    virtual void OnGameSettingsInitialized();
    
    // Запуск таймера, связанного с игровой логикой
    UFUNCTION(BlueprintCallable, Category = "MedCom|GameMode")
    void StartGameTimer(float InGameDuration);
    
    // Остановка таймера игры
    UFUNCTION(BlueprintCallable, Category = "MedCom|GameMode")
    void StopGameTimer();
    
    // Проверяет, есть ли условия для начала игры
    virtual bool CanGameStart() const;
    
    // Проверяет, должна ли игра завершиться
    virtual bool ShouldGameEnd() const;
    
    // Проверяет условия победы
    virtual void CheckWinConditions();
    
    // Обработчик изменения состояния матча
    virtual void OnMatchStateChanged(EMedComMatchState OldState, EMedComMatchState NewState);
    
protected:
    // Флаг готовности к старту игры
    UPROPERTY(VisibleAnywhere, Category = "MedCom|GameMode")
    bool bReadyToStart;
    
    // Флаг завершения игры
    UPROPERTY(VisibleAnywhere, Category = "MedCom|GameMode")
    bool bGameIsOver;
    
    // Максимальное число игроков
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|GameMode")
    int32 MaxPlayers;
    
    // Продолжительность игры (0 = неограниченно)
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|GameMode")
    float GameDuration;
    
    // Класс PlayerState по умолчанию
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Classes")
    TSubclassOf<class APlayerState> DefaultPlayerStateClass;
    
    // Класс HUD по умолчанию
    UPROPERTY(EditDefaultsOnly, Category = "MedCom|Classes")
    TSubclassOf<class AHUD> DefaultHUDClass;
    
    // Время начала игры
    FDateTime GameStartTime;
    
    // Таймер игры
    FTimerHandle GameTimerHandle;
    
private:
    // Обработчик таймера игры
    void OnGameTimerTick();
};