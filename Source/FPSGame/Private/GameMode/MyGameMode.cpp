#include "GameMode/MyGameMode.h"
#include "Character/EnemyCharacter.h"
#include "FPSGame/FPSGameCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerState/MyPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"

AMyGameMode::AMyGameMode()
{
    // 设置默认玩家状态类
    PlayerStateClass = AMyPlayerState::StaticClass();

    // 设置默认Pawn类
    DefaultPawnClass = AFPSGameCharacter::StaticClass();

    // 初始化游戏参数
    MaxEnemies = 6;        // 场上最多存在敌人数量
    CurrentEnemyCount = 0; // 当前敌人数量
    SpawnInterval = 3.0f;  // 生成敌人时间间隔
    GameDuration = 180.0f; // 游戏总时长
    RemainingTime = GameDuration;
    CurrentAlivePlayers = 0; // 初始存活玩家数

    // 设置为可网络旅行
    bUseSeamlessTravel = true;
}

void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 查找玩家重生点
    FindPlayerStarts();

    // 查找所有标记为"EnemySpawnPoint"的生成点
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), "EnemySpawnPoint", FoundActors);
    SpawnPoints = FoundActors;

    // 开始生成敌人
    GetWorld()->GetTimerManager().SetTimer(SpawnEnemyTimerHandle, this, &AMyGameMode::SpawnEnemy, SpawnInterval, true);

    // 开始游戏计时
    GetWorld()->GetTimerManager().SetTimer(GameTimerHandle, this, &AMyGameMode::UpdateGameTime, 1.0f, true);

    // 定期检查胜利者
    GetWorld()->GetTimerManager().SetTimer(CheckWinnerTimerHandle, this, &AMyGameMode::CheckForWinner, 5.0f, true);

    UE_LOG(LogTemp, Warning, TEXT("游戏开始！找到 %d 个玩家重生点"), PlayerStarts.Num());
}

void AMyGameMode::FindPlayerStarts()
{
    PlayerStarts.Empty();

    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        PlayerStarts.Add(*It);
        UE_LOG(LogTemp, Log, TEXT("找到玩家重生点: %s 位置: %s"),
            *It->GetName(), *It->GetActorLocation().ToString());
    }
}

void AMyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    UE_LOG(LogTemp, Warning, TEXT("=== 玩家登录 ==="));
    UE_LOG(LogTemp, Warning, TEXT("新玩家控制器: %s"), *NewPlayer->GetName());
    UE_LOG(LogTemp, Warning, TEXT("网络角色: %s"),
        NewPlayer->GetLocalRole() == ROLE_Authority ? TEXT("Authority") : TEXT("Client"));

    // 设置玩家名称
    if (NewPlayer->PlayerState)
    {
        int32 PlayerId = NewPlayer->PlayerState->GetPlayerId();
        FString PlayerName = FString::Printf(TEXT("Player%d"), PlayerId);
        NewPlayer->PlayerState->SetPlayerName(PlayerName);
        UE_LOG(LogTemp, Warning, TEXT("玩家ID: %d, 名称: %s"),
            PlayerId, *PlayerName);
    }

    // 生成玩家角色
    SpawnPlayerCharacter(NewPlayer);

    // 更新存活玩家数量
    CurrentAlivePlayers++;
    UE_LOG(LogTemp, Warning, TEXT("当前存活玩家: %d"), CurrentAlivePlayers);
}

void AMyGameMode::Logout(AController* Exiting)
{
    UE_LOG(LogTemp, Warning, TEXT("玩家离开: %s"), *Exiting->GetName());

    // 更新存活玩家数量
    CurrentAlivePlayers = FMath::Max(0, CurrentAlivePlayers - 1);

    Super::Logout(Exiting);
}

void AMyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    // 调用父类实现
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);

    UE_LOG(LogTemp, Warning, TEXT("处理新玩家: %s"), *NewPlayer->GetName());

    // 确保玩家有角色
    if (!NewPlayer->GetPawn())
    {
        SpawnPlayerCharacter(NewPlayer);
    }
}

// 自定义的玩家出生点选择函数
AActor* AMyGameMode::ChoosePlayerStartForController(AController* Player)
{
    if (PlayerStarts.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("没有找到玩家重生点！"));
        return nullptr;
    }

    // 简单轮询选择重生点
    static int32 CurrentStartIndex = 0;
    APlayerStart* ChosenStart = PlayerStarts[CurrentStartIndex % PlayerStarts.Num()];
    CurrentStartIndex++;

    UE_LOG(LogTemp, Log, TEXT("为玩家 %s 选择重生点: %s"),
        Player ? *Player->GetName() : TEXT("未知"),
        *ChosenStart->GetName());

    return ChosenStart;
}

void AMyGameMode::SpawnPlayerCharacter(APlayerController* PlayerController)
{
    if (!PlayerController || bGameEnded) return;

    UE_LOG(LogTemp, Warning, TEXT("=== 生成玩家角色 ==="));
    UE_LOG(LogTemp, Warning, TEXT("为控制器 %s 生成角色"), *PlayerController->GetName());

    // 如果已有Pawn，先销毁
    if (APawn* ExistingPawn = PlayerController->GetPawn())
    {
        UE_LOG(LogTemp, Warning, TEXT("销毁现有Pawn: %s"), *ExistingPawn->GetName());
        ExistingPawn->Destroy();
    }

    // 选择重生点（使用我们自定义的函数）
    AActor* StartSpot = ChoosePlayerStartForController(PlayerController);
    if (!StartSpot)
    {
        UE_LOG(LogTemp, Error, TEXT("无法找到重生点！"));
        StartSpot = GetWorld()->GetFirstPlayerController(); // 备用方案
    }

    // 确定要生成的Pawn类
    UClass* PawnClassToSpawn = DefaultPawnClass;
    if (PlayerClass && PlayerClass->IsChildOf(AFPSGameCharacter::StaticClass()))
    {
        PawnClassToSpawn = PlayerClass;
    }

    // 生成新角色
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = PlayerController;
    SpawnParams.Instigator = nullptr;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    FVector SpawnLocation = StartSpot ? StartSpot->GetActorLocation() : FVector::ZeroVector;
    FRotator SpawnRotation = StartSpot ? StartSpot->GetActorRotation() : FRotator::ZeroRotator;

    UE_LOG(LogTemp, Warning, TEXT("生成位置: %s"), *SpawnLocation.ToString());

    AFPSGameCharacter* NewCharacter = GetWorld()->SpawnActor<AFPSGameCharacter>(
        PawnClassToSpawn,
        SpawnLocation,
        SpawnRotation,
        SpawnParams
    );

    if (NewCharacter)
    {
        // 赋予控制权
        PlayerController->Possess(NewCharacter);

        UE_LOG(LogTemp, Warning, TEXT("成功生成角色: %s (地址: %p)"),
            *NewCharacter->GetName(), NewCharacter);
        UE_LOG(LogTemp, Warning, TEXT("角色网络角色: %s"),
            *UEnum::GetValueAsString(NewCharacter->GetLocalRole()));
        UE_LOG(LogTemp, Warning, TEXT("初始血量: %.0f/%.0f"),
            NewCharacter->GetCurrentHealth(), NewCharacter->GetMaxHealth());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("生成角色失败！"));
    }
}


//生成敌人
void AMyGameMode::SpawnEnemy()
{
    // 游戏已结束则不生成敌人
    if (bGameEnded) return;

    if (CurrentEnemyCount >= MaxEnemies || !EnemyClass || SpawnPoints.Num() == 0)
        return;

    // 随机选择一个生成点
    int32 RandomIndex = FMath::RandRange(0, SpawnPoints.Num() - 1);
    AActor* SpawnPoint = SpawnPoints[RandomIndex];

    if (SpawnPoint)
    {
        // 生成随机旋转（0-360度）
        FRotator RandomRotation(0, FMath::RandRange(0.0f, 360.0f), 0);

        // 生成敌人
        AEnemyCharacter* Enemy = GetWorld()->SpawnActor<AEnemyCharacter>(EnemyClass, SpawnPoint->GetActorLocation(), RandomRotation);
        if (Enemy)
        {
            CurrentEnemyCount++;
            UE_LOG(LogTemp, Log, TEXT("生成敌人，当前数量: %d"), CurrentEnemyCount);
        }
    }
}

void AMyGameMode::OnEnemyDeath(AEnemyCharacter* DeadEnemy)
{
    if (!DeadEnemy || bGameEnded)
        return;

    UE_LOG(LogTemp, Warning, TEXT("=== MyGameMode::OnEnemyDeath 被调用 ==="));
    UE_LOG(LogTemp, Warning, TEXT("死亡敌人: %s"), *DeadEnemy->GetName());

    // 获取击杀者控制器
    AController* KillerController = DeadEnemy->GetEnemyKiller();

    if (KillerController)
    {
        UE_LOG(LogTemp, Warning, TEXT("击杀者: %s"), *KillerController->GetName());

        // 尝试转换为玩家控制器
        APlayerController* PlayerController = Cast<APlayerController>(KillerController);
        if (PlayerController)
        {
            UE_LOG(LogTemp, Warning, TEXT("击杀者是玩家控制器"));

            // 获取击杀者的玩家状态
            AMyPlayerState* KillerPlayerState = KillerController->GetPlayerState<AMyPlayerState>();
            if (KillerPlayerState)
            {
                UE_LOG(LogTemp, Warning, TEXT("找到玩家状态: %s, 当前得分: %f"),
                    *KillerPlayerState->GetPlayerName(), KillerPlayerState->GetScore());

                // 调用 RPC 给击杀者加5分
                KillerPlayerState->AddPlayerScore(5);

                UE_LOG(LogTemp, Warning, TEXT("玩家 %s 击杀敌人，获得5分"),
                    *KillerPlayerState->GetPlayerName());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("无法获取击杀者的玩家状态！"));
            }
        }
        else
        {
            // 可能是AI控制器
            UE_LOG(LogTemp, Warning, TEXT("击杀者不是玩家控制器，是: %s"),
                *KillerController->GetClass()->GetName());
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("没有找到击杀者！"));

        // 尝试获取第一个玩家作为测试
        APlayerController* TestPC = GetWorld()->GetFirstPlayerController();
        if (TestPC)
        {
            UE_LOG(LogTemp, Warning, TEXT("作为测试，给第一个玩家 %s 加分"),
                *TestPC->GetName());

            AMyPlayerState* TestPS = TestPC->GetPlayerState<AMyPlayerState>();
            if (TestPS)
            {
                TestPS->AddPlayerScore(5);
            }
        }
    }

    // 减少敌人计数
    CurrentEnemyCount = FMath::Max(0, CurrentEnemyCount - 1);
    UE_LOG(LogTemp, Log, TEXT("敌人死亡，当前存活敌人数量: %d/%d"), CurrentEnemyCount, MaxEnemies);
}

void AMyGameMode::OnPlayerDeath(AFPSGameCharacter* DeadPlayer)
{
    if (!DeadPlayer || bGameEnded) return;

    CurrentAlivePlayers--;
    UE_LOG(LogTemp, Warning, TEXT("玩家 %s 死亡！当前存活玩家: %d"), *DeadPlayer->GetName(), CurrentAlivePlayers);

    // 立即检查游戏是否结束
    CheckForWinner();
}

void AMyGameMode::CheckForWinner()
{
    // 游戏已结束则不检查
    if (bGameEnded) return;

    TArray<AFPSGameCharacter*> AlivePlayers = GetAlivePlayers();

    // 条件1：时间耗尽
    if (RemainingTime <= 0)
    {
        EndGame(FString::Printf(TEXT("时间到！")));
        return;
    }

    // 条件2：只有1个玩家存活
    if (AlivePlayers.Num() == 1)
    {
        AFPSGameCharacter* WinnerPlayer = AlivePlayers[0];
        if (WinnerPlayer)
        {
            AMyPlayerState* WinnerPS = WinnerPlayer->GetPlayerState<AMyPlayerState>();
            if (WinnerPS)
            {
                EndGame(FString::Printf(TEXT("玩家 %s 胜利！"), *WinnerPS->GetPlayerName()));
            }
        }
        return;
    }

    // 条件3：没有玩家存活
    if (AlivePlayers.Num() == 0)
    {
        EndGame(FString::Printf(TEXT("所有玩家死亡！")));
        return;
    }
}
//得分统计
void AMyGameMode::PrintScoreboard()
{
    UE_LOG(LogTemp, Warning, TEXT("========== 得分榜 =========="));

    // 遍历所有玩家控制器
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC)
        {
            AMyPlayerState* PS = PC->GetPlayerState<AMyPlayerState>();
            if (PS)
            {
                UE_LOG(LogTemp, Warning, TEXT("玩家: %s - 得分: %f"),
                    *PS->GetPlayerName(),
                    PS->GetScore());
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("============================"));
}

//测试给指定玩家加分
void AMyGameMode::AddScoreToPlayer(int32 PlayerIndex, int32 Score)
{
    int32 Count = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && Count == PlayerIndex)
        {
            AMyPlayerState* PS = PC->GetPlayerState<AMyPlayerState>();
            if (PS)
            {
                PS->AddPlayerScore(Score);
                UE_LOG(LogTemp, Warning, TEXT("给玩家 %s 添加 %d 分，当前得分: %f"),
                    *PS->GetPlayerName(), Score, PS->GetScore());
                return;
            }
        }
        Count++;
    }

    UE_LOG(LogTemp, Warning, TEXT("未找到玩家索引: %d"), PlayerIndex);
}

// 游戏结束时添加一个调试命令
void AMyGameMode::DebugScore()
{
    PrintScoreboard();
}

void AMyGameMode::UpdateGameTime()
{
    if (bGameEnded) return;

    RemainingTime = FMath::Max(0.0f, RemainingTime - 1.0f);

    // 每秒显示剩余秒数
    int32 Minutes = FMath::FloorToInt(RemainingTime / 60.0f);
    int32 Seconds = FMath::FloorToInt(FMath::Fmod(RemainingTime, 60.0f));

    // 如果还有分钟数，显示MM:SS格式
    if (Minutes > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("剩余时间: %02d:%02d"), Minutes, Seconds);
    }
    else
    {
        // 只剩秒数时，只显示秒
        UE_LOG(LogTemp, Log, TEXT("剩余时间: %d秒"), Seconds);
    }

    if (RemainingTime <= 0)
    {
        EndGame(FString::Printf(TEXT("时间到！")));
    }
}

void AMyGameMode::TestScore()
{
    UE_LOG(LogTemp, Warning, TEXT("=== 测试加分功能 ==="));

    // 遍历所有玩家控制器
    int32 PlayerCount = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC)
        {
            UE_LOG(LogTemp, Warning, TEXT("玩家控制器 %d: %s"), PlayerCount, *PC->GetName());

            AMyPlayerState* PS = PC->GetPlayerState<AMyPlayerState>();
            if (PS)
            {
                UE_LOG(LogTemp, Warning, TEXT("  玩家状态: %s, 当前得分: %f"),
                    *PS->GetPlayerName(), PS->GetScore());

                // 测试加分
                PS->AddPlayerScore(10);
                UE_LOG(LogTemp, Warning, TEXT("  测试加分10分，新得分: %f"), PS->GetScore());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("  没有找到玩家状态！"));
            }
            PlayerCount++;
        }
    }
}

void AMyGameMode::AddTestScoreToAll()
{
    UE_LOG(LogTemp, Warning, TEXT("=== 给所有玩家加测试分 ==="));

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC)
        {
            AMyPlayerState* PS = PC->GetPlayerState<AMyPlayerState>();
            if (PS)
            {
                PS->AddPlayerScore(5);
                UE_LOG(LogTemp, Warning, TEXT("玩家 %s 获得5分，当前得分: %f"),
                    *PS->GetPlayerName(), PS->GetScore());
            }
        }
    }
}

void AMyGameMode::TestKill()
{
    UE_LOG(LogTemp, Warning, TEXT("=== 模拟击杀测试 ==="));

    // 找到第一个玩家控制器
    APlayerController* FirstPC = nullptr;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        FirstPC = It->Get();
        if (FirstPC) break;
    }

    if (FirstPC)
    {
        // 调用 OnEnemyDeath 函数，传入一个虚拟的击杀者
        UE_LOG(LogTemp, Warning, TEXT("模拟玩家 %s 击杀敌人"), *FirstPC->GetName());

        // 创建一个临时的敌人指针（实际不创建敌人，只是测试逻辑）
        AEnemyCharacter* DummyEnemy = nullptr;

        // 直接调用加分逻辑
        AMyPlayerState* PS = FirstPC->GetPlayerState<AMyPlayerState>();
        if (PS)
        {
            PS->AddPlayerScore(5);
            UE_LOG(LogTemp, Warning, TEXT("模拟击杀成功，玩家 %s 获得5分"), *PS->GetPlayerName());
        }
    }
}

void AMyGameMode::EndGame(const FString& EndReason)
{
    if (bGameEnded) return;

    bGameEnded = true;

    // 停止所有定时器
    GetWorld()->GetTimerManager().ClearTimer(SpawnEnemyTimerHandle);
    GetWorld()->GetTimerManager().ClearTimer(GameTimerHandle);

    // 获取存活玩家
    TArray<AFPSGameCharacter*> AlivePlayers = GetAlivePlayers();

    if (AlivePlayers.Num() == 1)
    {
        // 有胜利者
        AFPSGameCharacter* Winner = AlivePlayers[0];
        if (Winner)
        {
            AMyPlayerState* WinnerPS = Winner->GetPlayerState<AMyPlayerState>();
            if (WinnerPS)
            {
                WinnerPS->SetIsWinner(true);
                UE_LOG(LogTemp, Warning, TEXT("[游戏结束] %s 胜利者: %s (得分: %f)"),
                    *EndReason, *WinnerPS->GetPlayerName(), WinnerPS->GetScore());
            }
        }
    }
    else if (AlivePlayers.Num() > 1)
    {
        // 多个玩家存活，比较得分
        AMyPlayerState* Winner = nullptr;
        float HighestScore = -1.0f;

        for (AFPSGameCharacter* Player : AlivePlayers)
        {
            if (Player)
            {
                AMyPlayerState* PS = Player->GetPlayerState<AMyPlayerState>();
                if (PS && PS->GetScore() > HighestScore)
                {
                    HighestScore = PS->GetScore();
                    Winner = PS;
                }
            }
        }

        if (Winner)
        {
            Winner->SetIsWinner(true);
            UE_LOG(LogTemp, Warning, TEXT("[游戏结束] %s 胜利者: %s (最高得分: %f)"),
                *EndReason, *Winner->GetPlayerName(), Winner->GetScore());
        }
    }
    else
    {
        // 没有存活玩家
        UE_LOG(LogTemp, Warning, TEXT("[游戏结束] %s 没有胜利者"), *EndReason);
    }

    // 这里可以添加游戏结束的UI显示、数据统计等逻辑
    UE_LOG(LogTemp, Warning, TEXT("游戏结束！原因：%s"), *EndReason);
}

void AMyGameMode::DebugPlayers()
{
    UE_LOG(LogTemp, Warning, TEXT("======= 玩家调试信息 ======="));

    // 通过PlayerController遍历
    int32 PlayerIndex = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC)
        {
            UE_LOG(LogTemp, Warning, TEXT("玩家%d:"), PlayerIndex++);
            UE_LOG(LogTemp, Warning, TEXT("  控制器: %s"), *PC->GetName());
            UE_LOG(LogTemp, Warning, TEXT("  网络角色: %s"),
                *UEnum::GetValueAsString(PC->GetLocalRole()));

            if (PC->PlayerState)
            {
                UE_LOG(LogTemp, Warning, TEXT("  玩家名称: %s"), *PC->PlayerState->GetPlayerName());
            }

            AFPSGameCharacter* Character = Cast<AFPSGameCharacter>(PC->GetPawn());
            if (Character)
            {
                UE_LOG(LogTemp, Warning, TEXT("  角色: %s (地址: %p)"),
                    *Character->GetName(), Character);
                UE_LOG(LogTemp, Warning, TEXT("  角色网络角色: %s"),
                    *UEnum::GetValueAsString(Character->GetLocalRole()));
                UE_LOG(LogTemp, Warning, TEXT("  血量: %.0f/%.0f"),
                    Character->GetCurrentHealth(), Character->GetMaxHealth());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("  角色: 无"));
            }
        }
    }

    // 查找所有FPSGameCharacter实例
    TArray<AActor*> AllCharacters;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFPSGameCharacter::StaticClass(), AllCharacters);

    UE_LOG(LogTemp, Warning, TEXT("所有FPSGameCharacter实例 (%d 个):"), AllCharacters.Num());
    for (AActor* Actor : AllCharacters)
    {
        AFPSGameCharacter* Character = Cast<AFPSGameCharacter>(Actor);
        if (Character)
        {
            APlayerController* OwnerPC = Cast<APlayerController>(Character->GetController());
            FString OwnerName = OwnerPC ? OwnerPC->GetName() : TEXT("无控制器");

            UE_LOG(LogTemp, Warning, TEXT("  - %s (地址: %p)"),
                *Character->GetName(), Character);
            UE_LOG(LogTemp, Warning, TEXT("    控制器: %s"), *OwnerName);
            UE_LOG(LogTemp, Warning, TEXT("    网络角色: %s"),
                *UEnum::GetValueAsString(Character->GetLocalRole()));
            UE_LOG(LogTemp, Warning, TEXT("    血量: %.0f/%.0f"),
                Character->GetCurrentHealth(), Character->GetMaxHealth());
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("当前存活玩家数: %d"), CurrentAlivePlayers);
    UE_LOG(LogTemp, Warning, TEXT("游戏是否结束: %s"), bGameEnded ? TEXT("是") : TEXT("否"));
    UE_LOG(LogTemp, Warning, TEXT("================================="));
}

TArray<AFPSGameCharacter*> AMyGameMode::GetAlivePlayers()
{
    TArray<AFPSGameCharacter*> AlivePlayers;
    TArray<AActor*> AllPlayers;

    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFPSGameCharacter::StaticClass(), AllPlayers);

    for (AActor* Actor : AllPlayers)
    {
        AFPSGameCharacter* Player = Cast<AFPSGameCharacter>(Actor);
        if (Player && Player->GetCurrentHealth() > 0.0f)
        {
            AlivePlayers.Add(Player);
        }
    }

    return AlivePlayers;
}