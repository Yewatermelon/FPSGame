// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPSGameCharacter.h"
#include "GameMode/MyGameMode.h"
#include "FPSGameProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PawnMovementComponent.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Net/UnrealNetwork.h" 

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AFPSGameCharacter

AFPSGameCharacter::AFPSGameCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
		
	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	// 新增：初始化生命值
	CurrentHealth = MaxHealth;

}

//////////////////////////////////////////////////////////////////////////// Input

void AFPSGameCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AFPSGameCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFPSGameCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFPSGameCharacter::Look);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void AFPSGameCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add movement 
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void AFPSGameCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AFPSGameCharacter::BeginPlay()
{
	Super::BeginPlay();
	// 确保网络角色同步
	if (GetLocalRole() == ROLE_Authority)
	{
		CurrentHealth = MaxHealth;
	}
}

// 方式2：重写AActor的TakeDamage函数（标准方式）
float AFPSGameCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	// 首先记录谁调用了这个函数
	FString RoleString =
		GetLocalRole() == ROLE_Authority ? TEXT("Authority") :
		GetLocalRole() == ROLE_AutonomousProxy ? TEXT("AutonomousProxy") :
		GetLocalRole() == ROLE_SimulatedProxy ? TEXT("SimulatedProxy") : TEXT("None");

	UE_LOG(LogTemplateCharacter, Warning,
		TEXT("[TakeDamage] %s 被调用，角色: %s, 本地控制: %s, HasAuthority: %s"),
		*GetName(),
		*RoleString,
		IsLocallyControlled() ? TEXT("是") : TEXT("否"),
		HasAuthority() ? TEXT("是") : TEXT("否"));

	// 关键修改：只在服务器上处理伤害逻辑
	if (!HasAuthority())
	{
		// 客户端只记录日志，不处理伤害
		UE_LOG(LogTemplateCharacter, Log,
			TEXT("[客户端] %s 跳过伤害处理，等待服务器同步"),
			*GetName());
		return 0.0f;
	}

	// 调用父类的TakeDamage
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// 确保伤害值有效
	if (ActualDamage <= 0.0f)
	{
		UE_LOG(LogTemplateCharacter, Warning,
			TEXT("[服务器] %s 收到无效伤害: %.1f"),
			*GetName(), ActualDamage);
		return 0.0f;
	}

	// 仅在服务器处理伤害逻辑
	if (GetLocalRole() == ROLE_Authority)
	{
		// 记录受到伤害前的血量
		float HealthBefore = CurrentHealth;

		// 应用伤害
		CurrentHealth = FMath::Clamp(CurrentHealth - ActualDamage, 0.0f, MaxHealth);

		// 记录受到伤害后的血量
		float HealthAfter = CurrentHealth;

		// 添加剩余血量的日志
		UE_LOG(LogTemplateCharacter, Log,
			TEXT("%s 受到 %.1f 伤害，来源: %s，血量 %.0f -> %.0f (剩余 %.0f%%)"),
			*GetName(),
			ActualDamage,
			DamageCauser ? *DamageCauser->GetName() : TEXT("未知"),
			HealthBefore,
			HealthAfter,
			(HealthAfter / MaxHealth) * 100.0f
		);

		// 如果生命值耗尽，触发死亡
		if (CurrentHealth <= 0.0f)
		{
			UE_LOG(LogTemplateCharacter, Error, TEXT("[服务器] %s 死亡！"), *GetName());
			OnDeath();
		}
	}
	else
	{
		// 客户端记录但不处理
		UE_LOG(LogTemplateCharacter, Log,
			TEXT("[客户端] %s 收到伤害通知但不处理 (ActualDamage: %.1f)"),
			*GetName(), ActualDamage);
	}

	return ActualDamage;
}

// 新增：死亡处理
void AFPSGameCharacter::OnDeath()
{
	UE_LOG(LogTemplateCharacter, Warning, TEXT("%s 死亡，最终血量: %.0f/%.0f"),
		*GetName(), CurrentHealth, MaxHealth);

	// 通知GameMode
	if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
	{
		if (AMyGameMode* MyGameMode = Cast<AMyGameMode>(GameMode))
		{
			MyGameMode->OnPlayerDeath(this);
		}
	}

	// 可添加死亡动画、禁用输入等逻辑
	SetActorEnableCollision(false);
	GetMovementComponent()->StopMovementImmediately();
	// 通知游戏模式更新分数等（根据需求扩展）

	// 禁用输入
	DisableInput(GetController<APlayerController>());
}

void AFPSGameCharacter::OnRep_CurrentHealth()
{
	// 客户端收到血量更新
	UE_LOG(LogTemplateCharacter, Warning,
		TEXT("[客户端] %s 血量更新: %.0f/%.0f"),
		*GetName(), CurrentHealth, MaxHealth);

	// 如果血量<=0但还没死亡，调用OnDeath
	if (CurrentHealth <= 0.0f)
	{
		OnDeath();
	}
}

// 新增：网络同步设置
void AFPSGameCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFPSGameCharacter, CurrentHealth);
}

void AFPSGameCharacter::DebugNetworkInfo()
{
	UE_LOG(LogTemplateCharacter, Warning, TEXT("=== 网络信息调试 ==="));
	UE_LOG(LogTemplateCharacter, Warning, TEXT("角色: %s"), *GetName());
	UE_LOG(LogTemplateCharacter, Warning, TEXT("地址: %p"), this);
	UE_LOG(LogTemplateCharacter, Warning, TEXT("本地网络角色: %s"),
		GetLocalRole() == ROLE_Authority ? TEXT("Authority") :
		GetLocalRole() == ROLE_AutonomousProxy ? TEXT("AutonomousProxy") :
		GetLocalRole() == ROLE_SimulatedProxy ? TEXT("SimulatedProxy") : TEXT("None"));
	UE_LOG(LogTemplateCharacter, Warning, TEXT("远程网络角色: %s"),
		GetRemoteRole() == ROLE_Authority ? TEXT("Authority") :
		GetRemoteRole() == ROLE_AutonomousProxy ? TEXT("AutonomousProxy") :
		GetRemoteRole() == ROLE_SimulatedProxy ? TEXT("SimulatedProxy") : TEXT("None"));
	UE_LOG(LogTemplateCharacter, Warning, TEXT("是否本地控制: %s"),
		IsLocallyControlled() ? TEXT("是") : TEXT("否"));
	UE_LOG(LogTemplateCharacter, Warning, TEXT("是否有Authority: %s"),
		HasAuthority() ? TEXT("是") : TEXT("否"));
	UE_LOG(LogTemplateCharacter, Warning, TEXT("血量: %.0f/%.0f"),
		CurrentHealth, MaxHealth);
	UE_LOG(LogTemplateCharacter, Warning, TEXT("========================="));
}