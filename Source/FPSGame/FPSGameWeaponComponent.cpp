// Copyright Epic Games, Inc. All Rights Reserved.


#include "FPSGameWeaponComponent.h"
#include "FPSGameCharacter.h"
#include "FPSGameProjectile.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Animation/AnimInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h" 

// Sets default values for this component's properties
UFPSGameWeaponComponent::UFPSGameWeaponComponent()
{
	// Default offset from the character location for projectiles to spawn
	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);

	// 启用网络复制
	SetIsReplicatedByDefault(true);

	// 初始化指针
	Character = nullptr;
}

// 执行射击
void UFPSGameWeaponComponent::Fire()
{
	if (Character == nullptr || Character->GetController() == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Fire: 无效的角色或控制器"));
		return;
	}

	// 播放本地效果（客户端和服务器都执行）
	PlayLocalFireEffects();

	// 客户端先生成本地预测的子弹（视觉效果）
	if (!GetOwner()->HasAuthority())
	{
		// 客户端生成本地预测的子弹（只显示，不造成伤害）
		SpawnLocalPredictedProjectile();

		// 客户端调用RPC请求服务器生成实际的伤害子弹
		// 确保有网络连接
		if (Character->GetLocalRole() == ROLE_AutonomousProxy)
		{
			ServerFire();
			UE_LOG(LogTemp, Log, TEXT("[客户端] 发送射击RPC到服务器"));
		}
	}

	// 只在服务器上生成实际造成伤害的投射物
	if (GetOwner()->HasAuthority())
	{
		ServerFireProjectile();
	}
	else
	{
		// 客户端调用RPC请求服务器生成投射物
		ServerFire();
	}
}

// 生成本地预测的子弹（仅视觉效果）
void UFPSGameWeaponComponent::SpawnLocalPredictedProjectile()
{
	if (ProjectileClass == nullptr || !Character)
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
	if (PlayerController == nullptr)
	{
		return;
	}

	// 计算生成位置和旋转
	const FRotator SpawnRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
	const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);

	// 设置生成参数
	FActorSpawnParameters ActorSpawnParams;
	ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	ActorSpawnParams.Owner = Character;
	ActorSpawnParams.Instigator = Character;
	ActorSpawnParams.bNoFail = true;

	// 标记为本地生成，不复制到其他客户端
	ActorSpawnParams.bNoFail = true;
	ActorSpawnParams.ObjectFlags |= RF_Transient;

	// 在客户端上生成本地预测的子弹
	AFPSGameProjectile* Projectile = World->SpawnActor<AFPSGameProjectile>(
		ProjectileClass,
		SpawnLocation,
		SpawnRotation,
		ActorSpawnParams
	);

	if (Projectile)
	{
		// 客户端子弹设置为不复制，且不处理伤害
		Projectile->SetReplicates(false);
		Projectile->SetReplicateMovement(false);
		Projectile->bAlwaysRelevant = false;

		// 修改伤害为0，避免客户端意外造成伤害
		Projectile->DamageAmount = 0.0f;

		// 短暂存在后销毁（让服务器子弹接管）
		Projectile->SetLifeSpan(0.5f);

		UE_LOG(LogTemp, Log, TEXT("[客户端] 生成本地预测子弹: %s"), *Projectile->GetName());
	}
}

// 服务器RPC：客户端请求射击
void UFPSGameWeaponComponent::ServerFire_Implementation()
{
	// 服务器验证并生成投射物
	if (Character && Character->GetController())
	{
		ServerFireProjectile();

		// 广播给其他客户端播放效果（不包括射击者自己）
		if (!Character->IsLocallyControlled())
		{
			MulticastPlayFireEffects();
		}

		UE_LOG(LogTemp, Log, TEXT("[服务器] 处理射击RPC，生成投射物"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[服务器] 无效的角色或控制器，无法射击"));
	}
}

bool UFPSGameWeaponComponent::ServerFire_Validate()
{
	// 简单验证：确保有角色和控制器
	return Character != nullptr && Character->GetController() != nullptr;
}

// 服务器生成投射物
void UFPSGameWeaponComponent::ServerFireProjectile()
{
	if (ProjectileClass == nullptr)
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
	if (PlayerController == nullptr)
	{
		return;
	}

	// 计算生成位置和旋转
	const FRotator SpawnRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
	const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);

	// 设置生成参数
	FActorSpawnParameters ActorSpawnParams;
	ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	ActorSpawnParams.Owner = GetOwner(); // 设置所有者
	ActorSpawnParams.Instigator = Cast<APawn>(GetOwner()); // 设置发起者

	// 在服务器上生成投射物
	AFPSGameProjectile* Projectile = World->SpawnActor<AFPSGameProjectile>(
		ProjectileClass,
		SpawnLocation,
		SpawnRotation,
		ActorSpawnParams
	);

	if (Projectile)
	{
		// 设置投射物伤害等属性
		Projectile->SetOwner(GetOwner());

		UE_LOG(LogTemp, Log, TEXT("[服务器] 生成投射物: %s"), *Projectile->GetName());
	}
}

// 本地效果（客户端和服务器都执行）
void UFPSGameWeaponComponent::PlayLocalFireEffects()
{
	// 播放射击音效
	if (FireSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
	}

	// 播放射击动画
	if (FireAnimation != nullptr && Character->GetMesh1P())
	{
		UAnimInstance* AnimInstance = Character->GetMesh1P()->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}

	// 播放本地粒子效果等
	// ...

	UE_LOG(LogTemp, Log, TEXT("[本地] 播放射击效果"));
}

// 多播RPC：广播射击效果给所有客户端
void UFPSGameWeaponComponent::MulticastPlayFireEffects_Implementation()
{
	// 如果不是本地控制的角色，播放效果
	// （本地效果已经在PlayLocalFireEffects中播放过了）
	if (Character && !Character->IsLocallyControlled())
	{
		// 播放射击音效
		if (FireSound != nullptr)
		{
			UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
		}

		// 播放射击动画（如果是第一人称，可能不需要为其他玩家播放）
		// ...

		UE_LOG(LogTemp, Log, TEXT("[多播] 为远程玩家播放射击效果"));
	}
}


bool UFPSGameWeaponComponent::AttachWeapon(AFPSGameCharacter* TargetCharacter)
{
	UE_LOG(LogTemp, Log, TEXT("武器网络模式: %d, 角色网络角色: %d"),
		GetNetMode(),
		Character ? (int32)Character->GetLocalRole() : -1);

	Character = TargetCharacter;

	// Check that the character is valid, and has no weapon component yet
	if (Character == nullptr || Character->GetInstanceComponents().FindItemByClass<UFPSGameWeaponComponent>())
	{
		return false;
	}

	// Attach the weapon to the First Person Character
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(Character->GetMesh1P(), AttachmentRules, FName(TEXT("GripPoint")));

	// 确保组件在网络中正确注册
	if (GetNetMode() != NM_Standalone)
	{
		SetIsReplicated(true);
		// 设置复制移动（如果组件需要移动）
		if (GetAttachParent())
		{
			GetAttachParent()->SetIsReplicated(true);
		}
	}

	// Set up action bindings
	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
			Subsystem->AddMappingContext(FireMappingContext, 1);
		}

		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
		{
			// Fire
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UFPSGameWeaponComponent::Fire);
		}
	}


	UE_LOG(LogTemp, Log, TEXT("武器附加到角色: %s, 父组件: %s, 组件地址: %p"),
		*Character->GetName(),
		*GetNameSafe(GetAttachParent()),
		this);

	return true;
}

void UFPSGameWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ensure we have a character owner
	if (Character != nullptr)
	{
		// remove the input mapping context from the Player Controller
		if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
			{
				Subsystem->RemoveMappingContext(FireMappingContext);
			}
		}
	}

	// maintain the EndPlay call chain
	Super::EndPlay(EndPlayReason);
}

void UFPSGameWeaponComponent::InitializeNetworkOwnership(AFPSGameCharacter* OwnerCharacter)
{
	if (!OwnerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[武器] 初始化网络所有权失败: 无效的角色"));
		return;
	}

	Character = OwnerCharacter;

	// 确保组件有正确的网络设置
	if (GetNetMode() != NM_Standalone)
	{
		// 设置组件的网络复制
		SetIsReplicated(true);

		// 如果组件直接附加到角色，角色会自动处理网络所有权
		// 我们只需要确保组件的网络ID正确
		if (Character)
		{
			UE_LOG(LogTemp, Log, TEXT("[武器] 网络所有权初始化: 角色=%s, 网络角色=%s, 本地控制=%s"),
				*Character->GetName(),
				*UEnum::GetValueAsString(Character->GetLocalRole()),
				Character->IsLocallyControlled() ? TEXT("是") : TEXT("否"));
		}
	}
}

void UFPSGameWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 复制角色引用
	DOREPLIFETIME_CONDITION(UFPSGameWeaponComponent, Character, COND_OwnerOnly);
}