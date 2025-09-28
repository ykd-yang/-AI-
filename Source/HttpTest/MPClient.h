#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MPClient.generated.h"

USTRUCT(BlueprintType)
struct FMPHandPayload
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MPClient", meta=(JsonKey="hand_index"))
    int32 HandIndex = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MPClient", meta=(JsonKey="thumb_x"))
    TArray<float> ThumbX;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MPClient", meta=(JsonKey="index_x"))
    TArray<float> IndexX;
};

USTRUCT(BlueprintType)
struct FMPFramePayload
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MPClient", meta=(JsonKey="timestamp"))
    double Timestamp = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MPClient", meta=(JsonKey="hands"))
    TArray<FMPHandPayload> Hands;
};

DECLARE_LOG_CATEGORY_EXTERN(LogMPClient, Log, Warning);

UCLASS()
class AMPClient : public AActor
{
    GENERATED_BODY()

public:
    AMPClient();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(EditAnywhere, Category="MPClient")
    FString ServerAddress = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere, Category="MPClient")
    int32 ServerPort = 5555;

    UPROPERTY(EditAnywhere, Category="MPClient")
    float ReceiveTimeoutSeconds = 5.0f;

    bool ConnectToServer();
    bool ReceivePayload(FString& OutPayload);
    bool ParsePayload(const FString& InPayload, FMPFramePayload& OutData) const;
    void LogPayload(const FMPFramePayload& Payload) const;
    void CloseSocket();

    FSocket* ClientSocket;
};
