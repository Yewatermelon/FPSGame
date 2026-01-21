#include "Character/EnemyCharacter.h"
#include "GameMode/MyGameMode.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Components/SphereComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "FPSGame/FPSGameCharacter.h"
#include "PlayerState/MyPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h" 
#include "Kismet/KismetMathLibrary.h"

AEnemyCharacter::AEnemyCharacter()
{
    // 初始化碰撞（确保能被投射物命中）
    GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));

    // 启用网络复制
    bReplicates = true;

    // 强制AI控制器类（确保生成时自动绑定AI）
    AIControllerClass = AAIController::StaticClass();
    GetCharacterMovement()->bOrientRotationToMovement = true; // 移动时自动转向

    // 初始化攻击碰撞体（绑定到角色骨骼的"攻击点"）
    AttackCollision = CreateDefaultSubobject<USphereComponent>(TEXT("AttackCollision"));
    AttackCollision->InitSphereRadius(50.0f);
    AttackCollision->SetCollisionProfileName("EnemyAttack"); // 自定义碰撞配置
    AttackCollision->SetupAttachment(GetMesh(), FName("AttackSocket")); // 需在骨骼设置"AttackSocket"
    AttackCollision->OnComponentBeginOverlap.AddDynamic(this, &AEnemyCharacter::OnAttackCollisionOverlap);
    AttackCollision->SetActive(false); // 默认关闭

    // 确保碰撞通道设置正确（在项目设置中配置"EnemyAttack"通道与玩家碰撞）
    AttackCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // 初始化健康值
    MaxHealth = 100.0f;
    CurrentHealth = MaxHealth;

    // 战斗参数
    AttackRange = 150.0f;
    AttackDamage = 20.0f;
    AttackInterval = 1.0f;
    LastAttackTime = 0.0f;

    // AI参数
    ChaseRange = 1500.0f;
    bIsPatrolling = true;
    PatrolRadius = 500.0f;
    CurrentPatrolPointIndex = 0;

    // 死亡状态
    bIsDead = false;

    // 初始化击杀者控制器
    KillerInstigator = nullptr;
}

void AEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 如果还没有控制器，自动生成一个AI控制器
    if (!GetController() && GetLocalRole() == ROLE_Authority)
    {
        UE_LOG(LogTemp, Warning, TEXT("EnemyCharacter没有控制器，正在生成AI控制器..."));

        // 创建AI控制器
        AAIController* AIController = GetWorld()->SpawnActor<AAIController>(
            AAIController::StaticClass(),
            GetActorLocation(),
            GetActorRotation()
        );

        if (AIController)
        {
            AIController->Possess(this);
            UE_LOG(LogTemp, Log, TEXT("AI控制器已附加: %s"), *AIController->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("无法生成AI控制器！"));
        }
    }
    else if (GetController())
    {
        UE_LOG(LogTemp, Log, TEXT("敌人已有控制器: %s"), *GetController()->GetName());
    }
}

void AEnemyCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsDead) return;

    // 仅服务器处理攻击逻辑
    if (GetLocalRole() != ROLE_Authority) return;

    // 寻找范围内的玩家目标
    FindValidPlayerTarget();

    // 如果有目标且满足攻击条件，则执行攻击
    if (CurrentTargetPlayer && CanAttack())
    {
        AttackTarget();
    }
}

void AEnemyCharacter::SetEnemyKiller(AController* NewKiller)
{
    KillerControllerRef = NewKiller;
}

void AEnemyCharacter::OnAttackCollisionOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    AFPSGameCharacter* Player = Cast<AFPSGameCharacter>(OtherActor);
    if (Player && !bIsDead)
    {
        // 对玩家造成伤害
        UGameplayStatics::ApplyDamage(
            Player,
            AttackDamage,
            GetController(),
            this,
            UDamageType::StaticClass()
        );
    }
}

// 执行攻击逻辑
void AEnemyCharacter::AttackTarget()
{
    if (bIsDead || !CurrentTargetPlayer) return;

    LastAttackTime = GetWorld()->GetTimeSeconds();
    UE_LOG(LogTemp, Log, TEXT("敌人发起攻击，目标：%s"), *CurrentTargetPlayer->GetName());

    // 激活攻击碰撞体（持续短时间检测碰撞）
    AttackCollision->SetActive(true);
    // 0.5秒后关闭碰撞体（根据攻击动画时长调整）
    GetWorld()->GetTimerManager().SetTimer(
        AttackCollisionTimerHandle,  // 传递已有的TimerHandle
        [this]() { AttackCollision->SetActive(false); },
        0.5f,
        false
    );

    // 3. 转向目标（让敌人面对玩家）
    FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(
        GetActorLocation(),
        CurrentTargetPlayer->GetActorLocation()
    );
    LookAtRotation.Pitch = 0; // 只在水平方向转向
    SetActorRotation(LookAtRotation);
}


// 重写TakeDamage，接收子弹的ApplyDamage调用
float AEnemyCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // 调用父类的TakeDamage（UE默认逻辑）
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    // 记录造成伤害的控制器
    if (EventInstigator && ActualDamage > 0)
    {
        KillerControllerRef = EventInstigator;
    }

    // 仅服务器端执行扣血逻辑
    if (GetLocalRole() == ROLE_Authority)
    {
        Server_TakeDamage(ActualDamage, EventInstigator);
    }

    return ActualDamage;
}

void AEnemyCharacter::Server_TakeDamage_Implementation(float DamageAmount, AController* InstigatorController)
{
    if (bIsDead) return;

    // 验证伤害值合法性
    if (DamageAmount <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("无效伤害值：%.1f，忽略扣血"), DamageAmount);
        return;
    }

    CurrentHealth -= DamageAmount;
    UE_LOG(LogTemp, Log, TEXT("敌人受到伤害: %f, 剩余生命值: %f"), DamageAmount, CurrentHealth);

    // 记录击杀者控制器
    if (CurrentHealth <= 0.0f)
    {
        KillerInstigator = InstigatorController;
        Multicast_Die(KillerInstigator);
    }
}

bool AEnemyCharacter::Server_TakeDamage_Validate(float DamageAmount, AController* InstigatorController)
{
    return true;// 简单验证
}

void AEnemyCharacter::Multicast_Die_Implementation(AController* KillerController)
{
    // 确保只在服务器上执行
    if (GetLocalRole() != ROLE_Authority)
    {
        return;
    }

    // 输出击杀者信息
    if (KillerControllerRef)
    {
        UE_LOG(LogTemp, Warning, TEXT("敌人 %s 被玩家 %s 击杀"),
            *GetName(), *KillerControllerRef->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("敌人 %s 死亡，但没有记录击杀者"), *GetName());
    }

    bIsDead = true;

    // 停止所有移动
    AAIController* AIController = Cast<AAIController>(GetController());
    if (AIController)
    {
        AIController->StopMovement();
    }

    // 禁用碰撞
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AttackCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 播放死亡动画
    //PlayAnimMontage(DeathMontage);

    // 通知GameMode
    UWorld* World = GetWorld();
    if (World)
    {
        AMyGameMode* GameMode = Cast<AMyGameMode>(UGameplayStatics::GetGameMode(World));
        if (GameMode)
        {
            GameMode->OnEnemyDeath(this);
        }
    }

    //销毁尸体
    Destroy();
}

bool AEnemyCharacter::Multicast_Die_Validate(AController* KillerController)
{
    return true;
}

// 寻找攻击范围内的玩家目标
void AEnemyCharacter::FindValidPlayerTarget()
{
    TArray<AActor*> AllPlayers;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFPSGameCharacter::StaticClass(), AllPlayers);

    CurrentTargetPlayer = nullptr;
    float ClosestDistance = AttackRange; // 只检测攻击范围内的玩家

    for (AActor* Actor : AllPlayers)
    {
        AFPSGameCharacter* Player = Cast<AFPSGameCharacter>(Actor);
        if (Player && IsValid(Player))
        {
            // 计算与玩家的距离
            float Distance = FVector::Distance(GetActorLocation(), Player->GetActorLocation());
            // 找到最近的、在攻击范围内的玩家
            if (Distance <= ClosestDistance)
            {
                ClosestDistance = Distance;
                CurrentTargetPlayer = Player;
            }
        }
    }
}

// 检查是否满足攻击条件
bool AEnemyCharacter::CanAttack()
{
    if (!CurrentTargetPlayer || CurrentTargetPlayer->GetCurrentHealth() <= 0)
    {
        return false;
    }

    // 检查攻击间隔
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastAttackTime < AttackInterval)
    {
        return false;
    }

    // 再次确认距离（防止目标移动出范围）
    float Distance = FVector::Distance(GetActorLocation(), CurrentTargetPlayer->GetActorLocation());
    return Distance <= AttackRange;
}

void AEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 声明需要同步的属性（根据实际需求添加，例如生命值）
    DOREPLIFETIME(AEnemyCharacter, CurrentHealth);
    DOREPLIFETIME(AEnemyCharacter, bIsDead);
}