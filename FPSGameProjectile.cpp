// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPSGameProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Character/EnemyCharacter.h"
#include "FPSGame/FPSGameCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h" 

AFPSGameProjectile::AFPSGameProjectile() 
{
	// 启用网络复制
	bReplicates = true;
	SetReplicateMovement(true);

	// 优化网络复制设置
	bReplicateUsingRegisteredSubObjectList = true;
	SetNetUpdateFrequency(100.0f); // 更新频率
	SetMinNetUpdateFrequency(30.0f); 
	NetPriority = 2.0f; // 提高优先级

	// Use a sphere as a simple collision representation
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(5.0f);
	CollisionComp->BodyInstance.SetCollisionProfileName("Projectile");
	CollisionComp->OnComponentHit.AddDynamic(this, &AFPSGameProjectile::OnHit);		// set up a notification for when this component hits something blocking

	// 设置正确的复制条件
	CollisionComp->SetIsReplicated(true);

	// 只在服务器上注册碰撞事件
	if (GetLocalRole() == ROLE_Authority)
	{
		CollisionComp->OnComponentHit.AddDynamic(this, &AFPSGameProjectile::OnHit);
	}

	// Players can't walk on it
	CollisionComp->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.f));
	CollisionComp->CanCharacterStepUpOn = ECB_No;

	// Set as root component
	RootComponent = CollisionComp;

	// Use a ProjectileMovementComponent to govern this projectile's movement
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true;

	// 优化网络运动复制
	ProjectileMovement->SetIsReplicated(true);
	ProjectileMovement->SetNetAddressable(); // 允许网络寻址

	// Die after 3 seconds by default
	InitialLifeSpan = 3.0f;

	DamageAmount = 20.0f;
}

void AFPSGameProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 确保在服务器上处理碰撞
	if (GetLocalRole() != ROLE_Authority)
	{
		// 客户端上禁用碰撞检测
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// 客户端：如果这是服务器复制的子弹，确保它可见
		if (GetLocalRole() == ROLE_SimulatedProxy)
		{
			// 为模拟代理增加一些网络优化
			SetNetUpdateFrequency(60.0f);
		}
	}
	else
	{
		// 服务器：启用碰撞
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

void AFPSGameProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 只在服务器上处理伤害逻辑
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	// Only add impulse and destroy projectile if we hit a physics
	if ((OtherActor != nullptr) && (OtherActor != this)&& (OtherActor != GetOwner())/* && (OtherComp != nullptr) && OtherComp->IsSimulatingPhysics()*/)
	{
		//OtherComp->AddImpulseAtLocation(GetVelocity() * 100.0f, GetActorLocation());

		//UE_LOG(LogTemp, Warning, TEXT("[服务器投射物] 命中目标: %s"), *OtherActor->GetName());

		// 命中敌人：结算伤害
		AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(OtherActor);
		if (Enemy)
		{
			// 确保DamageAmount有值（避免伤害为0）
			if (DamageAmount <= 0) DamageAmount = 20.0f; // 临时默认值，后续可在蓝图中配置

			// 在应用伤害前记录击杀者
			AController* InstigatorController = GetInstigatorController();

			// 先在子弹中记录击杀者
			if (InstigatorController)
			{
				// 直接设置击杀者
				Enemy->SetEnemyKiller(InstigatorController);
				UE_LOG(LogTemp, Log, TEXT("[子弹] 为敌人 %s 设置击杀者: %s"),
					*Enemy->GetName(), *InstigatorController->GetName());
			}

			UGameplayStatics::ApplyDamage(
				Enemy,
				DamageAmount,
				InstigatorController,
				this, // 伤害来源（投射物自身）
				UDamageType::StaticClass()
			);
			// 如果敌人死亡，设置击杀者
			if (Enemy->GetCurrentHealth() <= 0.0f && InstigatorController)
			{
				UE_LOG(LogTemp, Log, TEXT("[服务器] 玩家 %s 击杀敌人 %s，获得5分"),
					*InstigatorController->GetName(),
					*Enemy->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("[服务器] 投射物命中敌人 %s，造成%.1f伤害！"),
					*Enemy->GetName(), DamageAmount);
			}

			// 销毁子弹
			Destroy();
			return;
		}

		// 命中其他玩家
		AFPSGameCharacter* Player = Cast<AFPSGameCharacter>(OtherActor);
		if (Player && Player != GetOwner())
		{
			if (DamageAmount <= 0) DamageAmount = 20.0f;

			UGameplayStatics::ApplyDamage(
				Player,
				DamageAmount,
				GetInstigatorController(),
				this,
				UDamageType::StaticClass()
			);
			UE_LOG(LogTemp, Log, TEXT("[服务器] 投射物命中玩家 %s，造成%.1f伤害！"),
				*Player->GetName(), DamageAmount);


			// 销毁子弹
			Destroy();
			return;
		}

		// 命中PhysicsActor预设的实体：立即销毁
		if (OtherComp && OtherComp->GetCollisionProfileName() == FName("PhysicsActor"))
		{
			// 添加物理冲量（对PhysicsActor施加力）
			if (OtherComp->IsSimulatingPhysics())
			{
				OtherComp->AddImpulseAtLocation(GetVelocity() * 100.0f, GetActorLocation());
				// 关键：复制物理状态
				if (OtherActor->GetIsReplicated())
				{
					// 强制更新物理对象的复制
					OtherComp->SetIsReplicated(true);
					OtherActor->ForceNetUpdate();
				}

				UE_LOG(LogTemp, Log, TEXT("[服务器] 投射物对 %s 施加物理力"), *OtherActor->GetName());
			}

			// 命中环境物体
			//UE_LOG(LogTemp, Log, TEXT("[服务器] 投射物命中环境物体: %s"), *OtherActor->GetName());

			Destroy(); // 强制销毁，不反弹
			return;
		}

		// 命中其他物体且有物理效果
		if (OtherComp && OtherComp->IsSimulatingPhysics())
		{
			OtherComp->AddImpulseAtLocation(GetVelocity() * 100.0f, GetActorLocation());
		}

		// 播放命中特效/音效
		// UGameplayStatics::PlaySoundAtLocation(this, HitSound, GetActorLocation());
		// UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitEffect, GetActorLocation());

		//Destroy();
	}
}

void AFPSGameProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 复制DamageAmount
	DOREPLIFETIME(AFPSGameProjectile, DamageAmount);
}