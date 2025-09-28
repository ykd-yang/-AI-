#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HttpRouteHandle.h"
#include "HttpResultCallback.h"
#include "HttpPoseServerActor.generated.h"

class AHttpPoseScoreActor;
class IHttpRouter;
struct FHttpServerRequest;

DECLARE_LOG_CATEGORY_EXTERN(LogHttpPoseServer, Log, All);

USTRUCT(BlueprintType)
struct FPoseDataPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HTTP")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "HTTP")
	double Confidence = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "HTTP")
	FString ClientId;

	UPROPERTY(BlueprintReadOnly, Category = "HTTP")
	double Timestamp = 0.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPoseDataReceived, const FPoseDataPayload&, Payload);

/**
 * Actor that encapsulates an HTTP server endpoint for receiving pose data payloads.
 * Mirrors the blueprint workflow documented in UnrealHttpServer_Blueprint.md while providing a C++ implementation.
 */
UCLASS()
class NEWIDOLPROJECT_API AHttpPoseServerActor : public AActor
{
	GENERATED_BODY()

public:
	AHttpPoseServerActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Allows manual injection of pose data using the raw JSON string. */
	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void ReceivePoseData(const FString& JsonData, const FString& ClientId, float Timestamp);

	/** Updates the score actor using the supplied message and confidence. */
	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void UpdateScore(const FString& Message, double Confidence);

	/** Generates a sample payload and routes it through the receive pipeline for testing. */
	UFUNCTION(BlueprintCallable, Category = "HTTP")
	void TestHttpRequest();

	/** Broadcast whenever a pose payload has been parsed on the game thread. */
	UPROPERTY(BlueprintAssignable, Category = "HTTP")
	FOnPoseDataReceived OnPoseDataReceived;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UTextRenderComponent* StatusText;

	/** Port used by the embedded HTTP server. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HTTP")
	int32 ListenPort = 4000;

	/** Route that accepts pose data payloads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HTTP")
	FString PoseRoute = TEXT("/pose_data");

	/** Automatically start the server on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HTTP")
	bool bAutoStart = true;

	/** Display transient debug strings on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HTTP")
	bool bShowDebugMessages = true;

	/** Optional reference to a score display actor placed in the level. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "HTTP")
	AHttpPoseScoreActor* ScoreDisplayActor = nullptr;

private:
	TSharedPtr<IHttpRouter> HttpRouter;
	FHttpRouteHandle PoseRouteHandle;
	bool bServerActive = false;

	bool StartHttpServer();
	void StopHttpServer();
	void ApplyStatusText(const FString& InStatus) const;
	void LogOnScreen(const FString& Message, const FColor& Colour = FColor::White) const;
	void HandlePosePayloadOnGameThread(const FString& JsonPayload, const FPoseDataPayload Payload);
	bool ParsePoseData(const FString& JsonPayload, FPoseDataPayload& OutPayload, FString& OutError) const;
	bool HandlePoseDataRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	FString BuildResponseJson(const FPoseDataPayload& Payload, bool bSuccess, const FString& ErrorMessage) const;
	void EnsureScoreActorReference();
	FString RouteForDisplay() const;

	/** Utility used by both HTTP and test entry points. */
	void DispatchPosePayload(const FString& JsonPayload, const FPoseDataPayload& Payload);
};
