// Copyright Suspense Team. All Rights Reserved.

#include "Core/SuspenseGameMode.h"
#include "Core/SuspensePlayerController.h"
#include "Core/SuspensePlayerState.h"
#include "Core/SuspenseGameState.h"
#include "Characters/SuspenseCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ASuspenseGameMode::ASuspenseGameMode()
    : bReadyToStart(false)
    , bGameIsOver(false)
    , MaxPlayers(16)
    , GameDuration(0.0f)
{
    // Устанавливаем базовые классы
    PlayerControllerClass = ASuspensePlayerController::StaticClass();
    DefaultPawnClass = ASuspenseCharacter::StaticClass();
    PlayerStateClass = ASuspensePlayerState::StaticClass();
    GameStateClass = ASuspenseGameState::StaticClass();
    
    // Включаем использование TravelFailure
    bUseSeamlessTravel = true;
}

void ASuspenseGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    
    // Инициализация стартовых настроек игры
    GameStartTime = FDateTime::Now();
    bGameIsOver = false;
    
    UE_LOG(LogTemp, Log, TEXT("SuspenseGameMode initialized on map: %s"), *MapName);
    
    // Вызов события инициализации настроек
    OnGameSettingsInitialized();
    
    // Подписываемся на изменения состояния матча
    ASuspenseGameState* SuspenseGS = GetGameState<ASuspenseGameState>();
    if (SuspenseGS)
    {
        SuspenseGS->OnMatchStateChangedDelegate.AddUObject(this, &ASuspenseGameMode::OnMatchStateChanged);
    }
}

FString ASuspenseGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
    FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
    
    // Вызов обработчика подключения нового игрока
    HandleNewPlayerConnection(NewPlayerController);
    
    return Result;
}

void ASuspenseGameMode::StartPlay()
{
    Super::StartPlay();
    
    UE_LOG(LogTemp, Log, TEXT("SuspenseGameMode StartPlay called"));
    
    // Устанавливаем состояние матча в "Ожидание старта"
    SetGameMatchState(ESuspenseMatchState::WaitingToStart);
    
    // Запуск игры, если все необходимые условия выполнены
    if (CanGameStart())
    {
        SetReadyToStart(true);
    }
    
    // Если задана продолжительность игры, запускаем таймер
    if (GameDuration > 0.0f)
    {
        StartGameTimer(GameDuration);
    }
}

void ASuspenseGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Останавливаем таймер игры при завершении
    StopGameTimer();
    
    // Установка состояния матча "Покидаем карту"
    SetGameMatchState(ESuspenseMatchState::LeavingMap);
    
    Super::EndPlay(EndPlayReason);
}

void ASuspenseGameMode::SetReadyToStart(bool bIsReady)
{
    if (bReadyToStart != bIsReady)
    {
        bReadyToStart = bIsReady;
        
        if (bReadyToStart)
        {
            // Вызываем событие готовности всех игроков
            OnAllPlayersReady();
        }
    }
}

bool ASuspenseGameMode::IsReadyToStart() const
{
    return bReadyToStart;
}

bool ASuspenseGameMode::IsGameOver() const
{
    return bGameIsOver;
}

void ASuspenseGameMode::FinishGame(AActor* Winner, const FString& EndGameReason)
{
    if (bGameIsOver)
    {
        return; // Игра уже завершена
    }
    
    bGameIsOver = true;
    
    UE_LOG(LogTemp, Log, TEXT("Game finished. Reason: %s"), *EndGameReason);
    
    // Останавливаем таймер игры
    StopGameTimer();
    
    // Вызов события окончания матча
    if (HasAuthority())
    {
        FString WinnerName = Winner ? Winner->GetName() : TEXT("None");
        UE_LOG(LogTemp, Log, TEXT("Game winner: %s"), *WinnerName);
        
        // Установка состояния матча в "Завершение игры"
        SetGameMatchState(ESuspenseMatchState::WaitingPostMatch);
        
        // Отправка события всем игрокам
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (APlayerController* PC = It->Get())
            {
                // Если у вас есть метод ClientGameEnded в вашем PlayerController
                // PC->ClientGameEnded(Winner, EndGameReason);
            }
        }
    }
}

void ASuspenseGameMode::SetGameMatchState(ESuspenseMatchState NewState)
{
    if (HasAuthority())
    {
        ASuspenseGameState* SuspenseGS = GetGameState<ASuspenseGameState>();
        if (SuspenseGS)
        {
            SuspenseGS->SetMatchState(NewState);
        }
    }
}

ESuspenseMatchState ASuspenseGameMode::GetGameMatchState() const
{
    ASuspenseGameState* SuspenseGS = GetGameState<ASuspenseGameState>();
    if (SuspenseGS)
    {
        return SuspenseGS->GetMatchState();
    }
    
    return ESuspenseMatchState::WaitingToStart; // Состояние по умолчанию
}

AActor* ASuspenseGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    // Сначала пробуем найти точки спавна через стандартный метод
    AActor* FoundPlayerStart = Super::ChoosePlayerStart_Implementation(Player);
    if (FoundPlayerStart)
    {
        return FoundPlayerStart;
    }
    
    // Если не нашли, перебираем все точки спавна и выбираем первую свободную
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }
    
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        APlayerStart* PlayerStart = *It;
        
        if (PlayerStart)
        {
            // Проверяем, не занята ли точка спавна кем-либо
            bool bIsOccupied = false;
            for (AController* Controller : TActorRange<AController>(World))
            {
                if (Controller && Controller->GetPawn())
                {
                    FVector PawnLocation = Controller->GetPawn()->GetActorLocation();
                    FVector StartLocation = PlayerStart->GetActorLocation();
                    if (FVector::Dist2D(PawnLocation, StartLocation) < 50.0f)
                    {
                        bIsOccupied = true;
                        break;
                    }
                }
            }
            
            if (!bIsOccupied)
            {
                return PlayerStart;
            }
        }
    }
    
    // Если все точки заняты, выбираем случайную
    TArray<APlayerStart*> AvailableStarts;
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        AvailableStarts.Add(*It);
    }
    
    if (AvailableStarts.Num() > 0)
    {
        return AvailableStarts[FMath::RandRange(0, AvailableStarts.Num() - 1)];
    }
    
    return nullptr;
}

void ASuspenseGameMode::HandleNewPlayerConnection(APlayerController* NewPlayer)
{
    if (!NewPlayer)
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("New player connected: %s"), *NewPlayer->GetName());
    
    // Проверяем, готовы ли все игроки к старту игры
    if (!bReadyToStart)
    {
        SetReadyToStart(CanGameStart());
    }
}

void ASuspenseGameMode::HandlePlayerDisconnection(APlayerController* DisconnectedPlayer)
{
    if (!DisconnectedPlayer)
    {
        return;
    }
    
    UE_LOG(LogTemp, Log, TEXT("Player disconnected: %s"), *DisconnectedPlayer->GetName());
    
    // Проверяем, нужно ли завершить игру при отключении игрока
    if (ShouldGameEnd())
    {
        FinishGame(nullptr, TEXT("All players disconnected"));
    }
}

void ASuspenseGameMode::OnAllPlayersReady()
{
    UE_LOG(LogTemp, Log, TEXT("All players are ready, game is starting"));
    
    // Установка состояния игры в прогресс
    if (HasAuthority())
    {
        SetGameMatchState(ESuspenseMatchState::InProgress);
    }
}

void ASuspenseGameMode::OnGameSettingsInitialized()
{
    UE_LOG(LogTemp, Log, TEXT("Game settings initialized"));
    
    // Здесь можно выполнить дополнительную настройку после инициализации
}

void ASuspenseGameMode::StartGameTimer(float InGameDuration)
{
    if (InGameDuration <= 0.0f)
    {
        return;
    }
    
    GameDuration = InGameDuration;
    
    // Запускаем таймер для проверки условий завершения игры
    GetWorldTimerManager().SetTimer(GameTimerHandle, this, &ASuspenseGameMode::OnGameTimerTick, 1.0f, true);
}

void ASuspenseGameMode::StopGameTimer()
{
    GetWorldTimerManager().ClearTimer(GameTimerHandle);
}

bool ASuspenseGameMode::CanGameStart() const
{
    // По умолчанию считаем, что игра может начаться, если есть хотя бы один игрок
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }
    
    int32 NumPlayers = World->GetNumPlayerControllers();
    return NumPlayers > 0;
}

bool ASuspenseGameMode::ShouldGameEnd() const
{
    // Проверяем, не превышено ли максимальное время игры
    if (GameDuration > 0.0f)
    {
        UWorld* World = GetWorld();
        if (!World)
        {
            return false;
        }
        
        float ElapsedTime = (FDateTime::Now() - GameStartTime).GetTotalSeconds();
        if (ElapsedTime >= GameDuration)
        {
            return true;
        }
    }
    
    // Проверка на отсутствие игроков
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }
    
    int32 NumPlayers = World->GetNumPlayerControllers();
    return NumPlayers == 0;
}

void ASuspenseGameMode::CheckWinConditions()
{
    // Базовая реализация не определяет победителя
    // Дочерние классы должны определить собственные условия победы
}

void ASuspenseGameMode::OnMatchStateChanged(ESuspenseMatchState OldState, ESuspenseMatchState NewState)
{
    UE_LOG(LogTemp, Log, TEXT("Match state changed from %s to %s"), 
           *UEnum::GetValueAsString(OldState), 
           *UEnum::GetValueAsString(NewState));
    
    // Дополнительная логика в зависимости от нового состояния
    switch (NewState)
    {
    case ESuspenseMatchState::InProgress:
        UE_LOG(LogTemp, Log, TEXT("Game started!"));
        break;
        
    case ESuspenseMatchState::WaitingPostMatch:
        UE_LOG(LogTemp, Log, TEXT("Game ended, waiting for post-match"));
        break;
        
    case ESuspenseMatchState::GameOver:
        UE_LOG(LogTemp, Log, TEXT("Game is officially over"));
        break;
        
    default:
        break;
    }
}

void ASuspenseGameMode::OnGameTimerTick()
{
    // Проверяем, не пора ли завершить игру
    if (ShouldGameEnd())
    {
        FinishGame(nullptr, TEXT("Time limit reached"));
        return;
    }
    
    // Проверяем условия победы
    CheckWinConditions();
}