// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MyGameInstance.generated.h"

UCLASS()
class FPSGAME_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
    UMyGameInstance();

    // 创建会话
    UFUNCTION(BlueprintCallable, Category = "Network")
    void CreateSession(int32 NumPublicConnections = 4, bool bUseLAN = false);

    // 加入会话
    UFUNCTION(BlueprintCallable, Category = "Network")
    void JoinSessionByIndex(int32 SessionIndex);

    // 查找会话
    UFUNCTION(BlueprintCallable, Category = "Network")
    void FindSessions(bool bUseLAN = false);

    // 销毁会话
    UFUNCTION(BlueprintCallable, Category = "Network")
    void DestroySession();

protected:
    // 会话接口
    IOnlineSessionPtr SessionInterface;

    // 会话搜索结果
    TSharedPtr<FOnlineSessionSearch> SessionSearch;

    // 会话名称
    FName SessionName;

    // 会话创建完成回调
    void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);

    // 会话查找完成回调
    void OnFindSessionsComplete(bool bWasSuccessful);

    // 会话加入完成回调
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

    // 会话销毁完成回调
    void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
};
