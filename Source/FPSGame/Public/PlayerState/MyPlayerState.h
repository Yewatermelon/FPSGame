#pragma once



#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MyPlayerState.generated.h"

UCLASS()
class FPSGAME_API AMyPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    AMyPlayerState();

    // 分数更新RPC（服务器端执行）
    UFUNCTION(Server, Reliable, WithValidation)
    void AddPlayerScore(int32 ScoreToAdd);
    virtual void AddPlayerScore_Implementation(int32 ScoreToAdd);
    virtual bool AddPlayerScore_Validate(int32 ScoreToAdd);

    // 获取当前分数 - 注意：不能加UFUNCTION，因为父类已有同名函数
    float GetPlayerScore() const { return PlayerScore; }

    // 蓝图可访问的获取分数函数
    UFUNCTION(BlueprintPure, Category = "Score")
    float GetPlayerScore_BP() const { return PlayerScore; }

    // 检查是否为胜利者
    UFUNCTION(BlueprintPure, Category = "Score")
    bool IsWinner() const { return bIsWinner; }

    // 设置为胜利者
    void SetIsWinner(bool bWinner) { bIsWinner = bWinner; }

protected:
    // 当前分数 - 使用不同的名称避免冲突
    UPROPERTY(Replicated, VisibleAnywhere, Category = "Score")
    float PlayerScore = 0.0f;

    // 是否为胜利者
    UPROPERTY(Replicated, VisibleAnywhere, Category = "Score")
    bool bIsWinner = false;

    // 分数更新时的回调
    UFUNCTION()
    void OnRep_PlayerScore();

public:
    // 必须重写的网络复制函数
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};