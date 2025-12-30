#pragma once
#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "GameFramework/Character.h"
#include "Perception/AIPerceptionTypes.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Components/SphereComponent.h"
#include "FPSGame/FPSGameCharacter.h"
#include "EnemyCharacter.generated.h"

UCLASS()
class FPSGAME_API AEnemyCharacter : public ACharacter, public IGenericTeamAgentInterface
{
    GENERATED_BODY()

public:
    AEnemyCharacter();

    // 重写UE的TakeDamage函数，接收子弹的ApplyDamage调用
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    // 服务器端处理伤害（RPC）
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_TakeDamage(float DamageAmount, AController* InstigatorController);
    virtual void Server_TakeDamage_Implementation(float DamageAmount, AController* KillerController);
    virtual bool Server_TakeDamage_Validate(float DamageAmount, AController* KillerController);

    // 多播通知死亡（RPC）
    UFUNCTION(NetMulticast, Reliable, WithValidation)
    void Multicast_Die(AController* KillerController);
    virtual void Multicast_Die_Implementation(AController* KillerController);
    virtual bool Multicast_Die_Validate(AController* KillerController);

    // 获取当前生命值
    UFUNCTION(BlueprintPure, Category = "Health")
    float GetCurrentHealth() const { return CurrentHealth; }

    // 获取最大生命值
    UFUNCTION(BlueprintCallable, Category = "Combat")
    float GetMaxHealth() const { return MaxHealth; }

    // 设置击杀者
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void SetEnemyKiller(AController* NewKiller);

    // 获取击杀者
    UFUNCTION(BlueprintCallable, Category = "Combat")
    AController* GetEnemyKiller() const { return KillerControllerRef; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 定时器句柄
    FTimerHandle AttackCollisionTimerHandle;

    //// 感知更新回调
    //UFUNCTION()
    //void OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors);

    // 攻击碰撞重叠回调
    UFUNCTION()
    void OnAttackCollisionOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    //// 移动到目标（蓝图/代码均可调用，适配AAIController）
    //UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    //void MoveToTarget(AActor* TargetActor);

    //寻找有效攻击玩家
    UFUNCTION(BlueprintCallable, Category = "AI|Targeting")
    void FindValidPlayerTarget();

    //是否可攻击
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    bool CanAttack();

    // 攻击目标（核心战斗函数，蓝图可直接调用）
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void AttackTarget();

private:
    // 攻击碰撞体
    UPROPERTY(VisibleAnywhere, Category = "Combat")
    class USphereComponent* AttackCollision;


    // 当前生命值
    UPROPERTY(Replicated, VisibleAnywhere, Category = "Health")
    float CurrentHealth;

    // 最大生命值
    UPROPERTY(EditAnywhere, Category = "Health")
    float MaxHealth;

    // 追击范围（蓝图可编辑）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = "true"))
    float ChaseRange;

    // 攻击范围
    UPROPERTY(EditAnywhere, Category = "Combat")
    float AttackRange;

    // 攻击伤害
    UPROPERTY(EditAnywhere, Category = "Combat")
    float AttackDamage;

    // 攻击间隔
    UPROPERTY(EditAnywhere, Category = "Combat")
    float AttackInterval;

    UPROPERTY()
    AController* KillerControllerRef = nullptr;

    // 上次攻击时间
    float LastAttackTime;

    // 新增：当前锁定的玩家目标
    UPROPERTY()
    AFPSGameCharacter* CurrentTargetPlayer;

    // 巡逻点
    UPROPERTY(EditAnywhere, Category = "AI")
    TArray<FVector> PatrolPoints;

    // 巡逻点最大数量（防止内存泄漏）
    UPROPERTY(EditAnywhere, Category = "AI")
    int32 MaxPatrolPoints = 5;

    // 当前巡逻点索引
    int32 CurrentPatrolPointIndex;

    // 巡逻半径
    UPROPERTY(EditAnywhere, Category = "AI")
    float PatrolRadius;

    // 是否在巡逻
    bool bIsPatrolling;

    // 死亡状态
    UPROPERTY(Replicated, VisibleAnywhere, Category = "Health")
    bool bIsDead;

    // 记录击杀者控制器（用于分数结算）
    AController* KillerInstigator;

    public:
        // 网络复制函数
        virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

};