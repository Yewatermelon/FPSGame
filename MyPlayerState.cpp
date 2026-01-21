#include "PlayerState/MyPlayerState.h"
#include "Net/UnrealNetwork.h" 

AMyPlayerState::AMyPlayerState()
{
    
}

void AMyPlayerState::AddPlayerScore_Implementation(int32 ScoreToAdd)
{
    // 仅在服务器端执行
    if (GetLocalRole() == ROLE_Authority)
    {
        PlayerScore += ScoreToAdd;
        // 添加详细的日志
        UE_LOG(LogTemp, Warning, TEXT("[MyPlayerState::AddPlayerScore] 玩家 %s 得分增加: %d, 新得分: %f"),
            *GetPlayerName(), ScoreToAdd, PlayerScore);

        // 强制网络更新
        ForceNetUpdate();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[MyPlayerState::AddPlayerScore] 客户端尝试加分，被拒绝"));
    }
}

bool AMyPlayerState::AddPlayerScore_Validate(int32 ScoreToAdd)
{
    // 验证分数是正数，防止作弊
    return ScoreToAdd > 0 && ScoreToAdd <= 1000;
}

void AMyPlayerState::OnRep_PlayerScore()
{
    // 客户端收到分数更新时的处理
    UE_LOG(LogTemp, Log, TEXT("客户端收到分数更新: %s 的新分数: %f"),
        *GetPlayerName(), PlayerScore);
}

void AMyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 声明需要同步的属性
    DOREPLIFETIME(AMyPlayerState, PlayerScore);
    DOREPLIFETIME(AMyPlayerState, bIsWinner);
}