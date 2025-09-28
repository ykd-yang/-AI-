#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"
#include "GameFramework/Actor.h"
#include "HttpClientModule.generated.h"

USTRUCT(BlueprintType)
struct FHttpClientProfile
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="HTTP")
    FString Name = TEXT("");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="HTTP")
    int32 Age = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="HTTP")
    float Height = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="HTTP")
    TArray<FString> Items;

    FString ToDebugString() const;
};

UCLASS()
class AHttpClientModule : public AActor
{
    GENERATED_BODY()

public:
    AHttpClientModule();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category="HTTP")
    FString EndpointUrl = TEXT("http://127.0.0.1:4000/user");

    UPROPERTY(EditAnywhere, Category="HTTP")
    FString RequestName = TEXT("\uC784\uAEBD\uC815");

private:
    void SendGetRequest();
    void SendPostRequest();
    FString BuildRequestPayload() const;
    void HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    bool ParseProfilesFromJson(const FString& Payload, TArray<FHttpClientProfile>& OutProfiles, bool& bOutHasProfiles) const;
    void LogProfiles(const FString& Context, const TArray<FHttpClientProfile>& Profiles) const;
    void HandleGetProfiles(const TArray<FHttpClientProfile>& Profiles, bool bHadProfiles);

    TArray<FHttpClientProfile> LatestProfiles;

public:
    virtual void Tick(float DeltaSeconds) override;
};