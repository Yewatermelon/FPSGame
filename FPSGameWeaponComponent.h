#pragma once
#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "FPSGameWeaponComponent.generated.h"

class AFPSGameCharacter;
class AFPSGameProjectile;
class UInputAction;
class UAnimMontage;

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FPSGAME_API UFPSGameWeaponComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()
public:

	/** Projectile class to spawn */
	UPROPERTY(EditDefaultsOnly, Category=Projectile)
	TSubclassOf<class AFPSGameProjectile> ProjectileClass;

	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	USoundBase* FireSound;
	
	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	UAnimMontage* FireAnimation;

	/** Gun muzzle's offset from the characters location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	FVector MuzzleOffset;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	class UInputMappingContext* FireMappingContext;

	/** Fire Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	class UInputAction* FireAction;

	/** Sets default values for this component's properties */
	UFPSGameWeaponComponent();

	/** Attaches the actor to a FirstPersonCharacter */
	UFUNCTION(BlueprintCallable, Category="Weapon")
	bool AttachWeapon(AFPSGameCharacter* TargetCharacter);

	/** Make the weapon Fire a Projectile */
	UFUNCTION(BlueprintCallable, Category="Weapon")
	void Fire();

	//生成本地预测的子弹（仅视觉效果）
	void SpawnLocalPredictedProjectile();

	// 初始化网络所有权（由角色在适当时机调用）
	void InitializeNetworkOwnership(AFPSGameCharacter* OwnerCharacter);
	
protected:
	/** Ends gameplay for this component. */
	UFUNCTION()
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 网络复制
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** The Character holding this weapon*/
	UPROPERTY(Replicated)
	AFPSGameCharacter* Character;

	// 服务器RPC：客户端请求射击
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerFire();
	void ServerFire_Implementation();
	bool ServerFire_Validate();

	// 服务器生成投射物
	void ServerFireProjectile();

	// 播放本地射击效果
	void PlayLocalFireEffects();

	// 多播RPC：广播射击效果给所有客户端
	void MulticastPlayFireEffects_Implementation();

	// 多播RPC：广播射击效果
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayFireEffects();

	/** 上次射击时间 */
	float LastFireTime = 0.0f;

	/** 上次服务器射击时间 */
	float LastServerFireTime = 0.0f;

	/** 射击间隔（秒） */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	float FireInterval = 0.2f; // 每秒5发
};
