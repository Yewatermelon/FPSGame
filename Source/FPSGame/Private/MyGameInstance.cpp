#include "MyGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemTypes.h" 
#include "Online/OnlineSessionNames.h"
#include "Interfaces/OnlineSessionInterface.h"

UMyGameInstance::UMyGameInstance()
{
    SessionName = FName("FPSGameSession");
}

void UMyGameInstance::CreateSession(int32 NumPublicConnections, bool bUseLAN)
{
    SessionInterface = IOnlineSubsystem::Get()->GetSessionInterface();

    if (SessionInterface.IsValid())
    {
        FOnlineSessionSettings SessionSettings;

        // 设置会话为LAN或在线
        if (IOnlineSubsystem::Get()->GetSubsystemName() == "NULL")
        {
            SessionSettings.bIsLANMatch = true;
        }
        else
        {
            SessionSettings.bIsLANMatch = bUseLAN;
        }

        SessionSettings.NumPublicConnections = NumPublicConnections;
        SessionSettings.bShouldAdvertise = true;
        SessionSettings.bAllowJoinInProgress = true;
        SessionSettings.bUsesPresence = true;
        SessionSettings.bUseLobbiesIfAvailable = true;

        // 设置地图名称
        SessionSettings.Set(FName("MapName"), FString("FirstPersonMap"), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

        SessionInterface->CreateSession(0, SessionName, SessionSettings);
        SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UMyGameInstance::OnCreateSessionComplete));
    }
}

void UMyGameInstance::FindSessions(bool bUseLAN)
{
    SessionInterface = IOnlineSubsystem::Get()->GetSessionInterface();

    if (SessionInterface.IsValid())
    {
        SessionSearch = MakeShareable(new FOnlineSessionSearch());
        SessionSearch->bIsLanQuery = bUseLAN;
        SessionSearch->MaxSearchResults = 10000;
        FOnlineSearchSettings& SearchSettings = SessionSearch->QuerySettings;
        SearchSettings.Set(FName(TEXT("GAMEMODE")), FString("FreeForAll"), EOnlineComparisonOp::Equals);
        //SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

        UE_LOG(LogTemp, Warning, TEXT("开始查找会话..."));
        SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
        SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UMyGameInstance::OnFindSessionsComplete));
    }
}

void UMyGameInstance::JoinSessionByIndex(int32 SessionIndex)
{
    if (!SessionInterface.IsValid() || !SessionSearch.IsValid() || SessionIndex >= SessionSearch->SearchResults.Num())
        return;

    SessionInterface->JoinSession(0, SessionName, SessionSearch->SearchResults[SessionIndex]);
    SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMyGameInstance::OnJoinSessionComplete));
}

void UMyGameInstance::DestroySession()
{
    if (SessionInterface.IsValid())
    {
        SessionInterface->DestroySession(SessionName);
        SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMyGameInstance::OnDestroySessionComplete));
    }
}

void UMyGameInstance::OnCreateSessionComplete(FName InSessionName, bool bWasSuccessful)
{
    FString ResultString = bWasSuccessful ? FString(TEXT("成功")) : FString(TEXT("失败"));
    UE_LOG(LogTemp, Warning, TEXT("会话创建 %s"), *ResultString);

    if (bWasSuccessful)
    {
        // 旅行到游戏地图
        UWorld* World = GetWorld();
        if (World)
        {
            World->ServerTravel("/Game/FirstPerson/Maps/FirstPersonMap?listen");
        }
    }
}

void UMyGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
    FString ResultString = FString(TEXT("成功"));
    UE_LOG(LogTemp, Warning, TEXT("查找会话 %s，找到 %d 个会话"),
        *ResultString, SessionSearch->SearchResults.Num());

    // 可以在这里更新UI显示找到的会话
}

void UMyGameInstance::OnJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type Result)
{
    UE_LOG(LogTemp, Warning, TEXT("加入会话结果: %d"), Result);

    if (Result == EOnJoinSessionCompleteResult::Success)
    {
        APlayerController* PlayerController = GetFirstLocalPlayerController();
        FString ConnectString;

        if (SessionInterface.IsValid() && SessionInterface->GetResolvedConnectString(InSessionName, ConnectString))
        {
            UE_LOG(LogTemp, Warning, TEXT("连接字符串: %s"), *ConnectString);
            PlayerController->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
        }
    }
}

void UMyGameInstance::OnDestroySessionComplete(FName InSessionName, bool bWasSuccessful)
{
    FString ResultString = bWasSuccessful ? FString(TEXT("成功")) : FString(TEXT("失败"));
    UE_LOG(LogTemp, Warning, TEXT("会话销毁 %s"), *ResultString);
}