#include "MPClient.h"

#include "Common/TcpSocketBuilder.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Components/TextRenderComponent.h"

DEFINE_LOG_CATEGORY(LogMPClient);

AMPClient::AMPClient()
    : ClientSocket(nullptr)
{
    PrimaryActorTick.bCanEverTick = false;
}

void AMPClient::BeginPlay()
{
    Super::BeginPlay();

    LastPayload = FMPFramePayload();
    LastTotalScore = 0.0f;

    EnsureScoreActorReference();

    if (!ConnectToServer())
    {
        UE_LOG(LogMPClient, Warning, TEXT("Failed to connect to %s:%d"), *ServerAddress, ServerPort);
        return;
    }

    FString Payload;
    if (!ReceivePayload(Payload))
    {
        UE_LOG(LogMPClient, Warning, TEXT("Did not receive JSON payload within %.2f seconds"), ReceiveTimeoutSeconds);
        return;
    }

    FMPFramePayload ParsedPayload;
    if (!ParsePayload(Payload, ParsedPayload))
    {
        UE_LOG(LogMPClient, Warning, TEXT("Received payload is not valid JSON: %s"), *Payload);
        return;
    }

    LogPayload(ParsedPayload);
    HandlePayload(ParsedPayload);
}

void AMPClient::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CloseSocket();
    Super::EndPlay(EndPlayReason);
}

bool AMPClient::ConnectToServer()
{
    if (ClientSocket)
    {
        return true;
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogMPClient, Warning, TEXT("Socket subsystem is unavailable"));
        return false;
    }

    ClientSocket = FTcpSocketBuilder(TEXT("MPClientSocket"))
        .AsBlocking()
        .AsReusable()
        .WithReceiveBufferSize(2 * 1024 * 1024);

    if (!ClientSocket)
    {
        UE_LOG(LogMPClient, Warning, TEXT("Failed to create TCP socket"));
        return false;
    }

    bool bIsValid = false;
    TSharedRef<FInternetAddr> RemoteAddress = SocketSubsystem->CreateInternetAddr();
    RemoteAddress->SetIp(*ServerAddress, bIsValid);

    if (!bIsValid)
    {
        UE_LOG(LogMPClient, Warning, TEXT("Server address %s is invalid"), *ServerAddress);
        CloseSocket();
        return false;
    }

    RemoteAddress->SetPort(ServerPort);

    if (!ClientSocket->Connect(*RemoteAddress))
    {
        UE_LOG(LogMPClient, Warning, TEXT("Unable to connect to %s:%d"), *ServerAddress, ServerPort);
        CloseSocket();
        return false;
    }

    ClientSocket->SetNonBlocking(false);

    return true;
}

bool AMPClient::ReceivePayload(FString& OutPayload)
{
    if (!ClientSocket)
    {
        return false;
    }

    TArray<uint8> Buffer;
    Buffer.Reserve(1024);

    const double StartTime = FPlatformTime::Seconds();
    double LastDataTime = StartTime;
    bool bReceivedAnyData = false;

    while ((FPlatformTime::Seconds() - StartTime) < ReceiveTimeoutSeconds)
    {
        uint32 PendingData = 0;
        if (ClientSocket->HasPendingData(PendingData) && PendingData > 0)
        {
            const int32 PreviousNum = Buffer.Num();
            Buffer.SetNum(PreviousNum + PendingData);
            int32 BytesRead = 0;
            if (ClientSocket->Recv(Buffer.GetData() + PreviousNum, PendingData, BytesRead))
            {
                if (static_cast<uint32>(BytesRead) < PendingData)
                {
                    Buffer.SetNum(PreviousNum + BytesRead);
                }

                bReceivedAnyData = true;
                LastDataTime = FPlatformTime::Seconds();
            }
            else
            {
                Buffer.SetNum(PreviousNum);
                break;
            }
        }
        else if (bReceivedAnyData)
        {
            if ((FPlatformTime::Seconds() - LastDataTime) > 0.1f)
            {
                break;
            }

            FPlatformProcess::Sleep(0.01f);
        }
        else
        {
            FPlatformProcess::Sleep(0.01f);
        }
    }

    if (!bReceivedAnyData)
    {
        return false;
    }

    Buffer.Add(0);
    const char* PayloadChars = reinterpret_cast<const char*>(Buffer.GetData());
    OutPayload = UTF8_TO_TCHAR(PayloadChars);
    return true;
}

bool AMPClient::ParsePayload(const FString& InPayload, FMPFramePayload& OutData) const
{
    return FJsonObjectConverter::JsonObjectStringToUStruct(InPayload, &OutData, 0, 0);
}

void AMPClient::LogPayload(const FMPFramePayload& Payload) const
{
    UE_LOG(LogMPClient, Warning, TEXT("timestamp=%.6f total_score=%.3f hands=%d"), Payload.Timestamp, Payload.TotalScore, Payload.Hands.Num());

    for (const FMPHandPayload& Hand : Payload.Hands)
    {
        const FString ThumbValues = FString::JoinBy(Hand.ThumbX, TEXT(", "), [](float Value)
        {
            return FString::SanitizeFloat(Value);
        });

        const FString IndexValues = FString::JoinBy(Hand.IndexX, TEXT(", "), [](float Value)
        {
            return FString::SanitizeFloat(Value);
        });

        UE_LOG(LogMPClient, Warning, TEXT("hand_index=%d thumb_x=[%s] index_x=[%s]"), Hand.HandIndex, *ThumbValues, *IndexValues);
    }
}

void AMPClient::CloseSocket()
{
    if (!ClientSocket)
    {
        return;
    }

    ClientSocket->Close();

    if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
    {
        SocketSubsystem->DestroySocket(ClientSocket);
    }

    ClientSocket = nullptr;
}

void AMPClient::HandlePayload(const FMPFramePayload& Payload)
{
    LastPayload = Payload;
    LastTotalScore = Payload.TotalScore;

    UpdateScoreDisplay(LastTotalScore);

    OnTotalScoreUpdated.Broadcast(LastTotalScore);
    OnTotalScoreUpdatedBP(LastTotalScore);
}

void AMPClient::EnsureScoreActorReference()
{
    if (ScoreDisplayActor && IsValid(ScoreDisplayActor))
    {
        return;
    }

    ScoreDisplayActor = nullptr;

    if (!GetWorld())
    {
        return;
    }

    TArray<AActor*> CandidateActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), CandidateActors);

    for (AActor* Actor : CandidateActors)
    {
        if (!IsValid(Actor))
        {
            continue;
        }

        const FString ClassName = Actor->GetClass()->GetName();
        if (ClassName.Contains(TEXT("BP_Score_C")))
        {
            ScoreDisplayActor = Actor;
            break;
        }
    }
}

void AMPClient::UpdateScoreDisplay(float Score)
{
    EnsureScoreActorReference();

    if (!ScoreDisplayActor || !IsValid(ScoreDisplayActor))
    {
        UE_LOG(LogMPClient, Verbose, TEXT("ScoreDisplayActor not assigned; skipping score update."));
        return;
    }

    UTextRenderComponent* TextComponent = ScoreDisplayActor->FindComponentByClass<UTextRenderComponent>();
    if (!TextComponent)
    {
        UE_LOG(LogMPClient, Warning, TEXT("Score actor %s has no TextRenderComponent."), *ScoreDisplayActor->GetName());
        return;
    }

    const int32 Precision = FMath::Clamp(ScoreDecimalPlaces, 0, 6);
    const FString FormattedScore = FString::Printf(TEXT("%.*f"), Precision, Score);
    const FString DisplayString = ScoreTextPrefix.IsEmpty() ? FormattedScore : (ScoreTextPrefix + FormattedScore);

    TextComponent->SetText(FText::FromString(DisplayString));
}

void AMPClient::SetScoreDisplayActor(AActor* InActor)
{
    ScoreDisplayActor = InActor;
    RefreshScoreDisplay();
}

void AMPClient::RefreshScoreDisplay()
{
    UpdateScoreDisplay(LastTotalScore);
}
