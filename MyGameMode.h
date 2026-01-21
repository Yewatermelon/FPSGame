#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MyGameMode.generated.h"
class AEnemyCharacter;
class AMyPlayerState;
UCLASS()
class FPSGAME_API AMyGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMyGameMode();

    // 必须重写的函数
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

    // 选择玩家出生点（这个不是虚函数，不需要override）
    AActor* ChoosePlayerStartForController(AController* Player);

    // 检查是否有胜利者
    void CheckForWinner();
    
    //得分统计
    void PrintScoreboard();

    // 生成敌人
    void SpawnEnemy();

    // 敌人死亡时的回调
    void OnEnemyDeath(AEnemyCharacter* DeadEnemy);

    // 玩家死亡时的处理
    void OnPlayerDeath(class AFPSGameCharacter* DeadPlayer);

    // 测试：给指定玩家加分
    UFUNCTION(Exec, Category = "Debug")
    void AddScoreToPlayer(int32 PlayerIndex, int32 Score);

    // 显示得分榜
    UFUNCTION(Exec, Category = "Debug")
    void DebugScore();

    // 调试函数
    UFUNCTION(Exec, Category = "Debug")
    void DebugPlayers();

    // 测试加分功能
    UFUNCTION(Exec, Category = "Debug")
    void TestScore();

    // 给所有玩家加测试分
    UFUNCTION(Exec, Category = "Debug")
    void AddTestScoreToAll();

    // 模拟击杀敌人测试
    UFUNCTION(Exec, Category = "Debug")
    void TestKill();

protected:
    // 敌人类型
    UPROPERTY(EditAnywhere, Category = "Enemy")
    TSubclassOf<class AEnemyCharacter> EnemyClass;

    // 玩家类型
    UPROPERTY(EditAnywhere, Category = "Game")
    TSubclassOf<class AFPSGameCharacter> PlayerClass;

    // 生成点数组
    UPROPERTY(EditAnywhere, Category = "Enemy")
    TArray<class AActor*> SpawnPoints;

    // 最大敌人数量
    UPROPERTY(EditAnywhere, Category = "Enemy")
    int32 MaxEnemies;

    // 当前敌人数量
    int32 CurrentEnemyCount;

    // 当前存活的玩家数量
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game")
    int32 CurrentAlivePlayers;

    // 生成敌人的时间间隔
    UPROPERTY(EditAnywhere, Category = "Enemy")
    float SpawnInterval;

    // 游戏持续时间（秒）
    UPROPERTY(EditAnywhere, Category = "Game")
    float GameDuration;

    // 剩余时间
    float RemainingTime;

    // 游戏是否已结束
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game")
    bool bGameEnded = false;

    // 玩家重生点数组
    UPROPERTY()
    TArray<class APlayerStart*> PlayerStarts;

    // 定时器句柄
    FTimerHandle SpawnEnemyTimerHandle;
    FTimerHandle CheckWinnerTimerHandle;
    FTimerHandle GameTimerHandle;

    // 更新游戏时间
    void UpdateGameTime();

    // 游戏结束
    void EndGame(const FString& EndReason);

    

    // 获取存活的玩家
    TArray<class AFPSGameCharacter*> GetAlivePlayers();

    // 玩家重生
    void SpawnPlayerCharacter(APlayerController* PlayerController);

    // 查找所有玩家重生点
    void FindPlayerStarts();
};