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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MPClient", meta=(JsonKey="total_score"))
    float TotalScore = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MPClient", meta=(JsonKey="hands"))
    TArray<FMPHandPayload> Hands;
};

DECLARE_LOG_CATEGORY_EXTERN(LogMPClient, Log, Warning);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTotalScoreUpdated, float, TotalScore);

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MPClient|Score", meta=(AllowPrivateAccess="true"))
    TObjectPtr<AActor> ScoreDisplayActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MPClient|Score", meta=(AllowPrivateAccess="true"))
    FString ScoreTextPrefix = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MPClient|Score", meta=(AllowPrivateAccess="true", ClampMin="0", ClampMax="6"))
    int32 ScoreDecimalPlaces = 1;

    bool ConnectToServer();
    bool ReceivePayload(FString& OutPayload);
    bool ParsePayload(const FString& InPayload, FMPFramePayload& OutData) const;
    void LogPayload(const FMPFramePayload& Payload) const;
    void CloseSocket();
    void HandlePayload(const FMPFramePayload& Payload);
    void EnsureScoreActorReference();
    void UpdateScoreDisplay(float Score);

    FSocket* ClientSocket;

    UPROPERTY(BlueprintReadOnly, Category="MPClient|Score", meta=(AllowPrivateAccess="true"))
    FMPFramePayload LastPayload;

    UPROPERTY(BlueprintReadOnly, Category="MPClient|Score", meta=(AllowPrivateAccess="true"))
    float LastTotalScore = 0.0f;

public:
    UPROPERTY(BlueprintAssignable, Category="MPClient|Score")
    FOnTotalScoreUpdated OnTotalScoreUpdated;

    UFUNCTION(BlueprintCallable, Category="MPClient|Score")
    void SetScoreDisplayActor(AActor* InActor);

    UFUNCTION(BlueprintCallable, Category="MPClient|Score")
    void RefreshScoreDisplay();

    UFUNCTION(BlueprintPure, Category="MPClient|Score")
    float GetLastTotalScore() const { return LastTotalScore; }

    UFUNCTION(BlueprintImplementableEvent, Category="MPClient|Score")
    void OnTotalScoreUpdatedBP(float NewScore);
};
