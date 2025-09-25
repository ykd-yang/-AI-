#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "DialogueNpcComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDialogueNPC, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCReply, const FString&, Text);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class NEWIDOLPROJECT_API UDialogueNpcComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC")
	FString ProxyURL = TEXT("http://127.0.0.1:8787/npc/say");

	UPROPERTY(BlueprintAssignable, Category="NPC")
	FOnNPCReply OnNPCReply;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC")
	bool bAutoSendOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC")
	FString DefaultPrompt = TEXT("안녕? 연결 테스트 중");

	UFUNCTION(BlueprintCallable, Category="NPC")
	void SendDefaultPrompt();

	UFUNCTION(BlueprintCallable, Category="NPC")
	void SendPrompt(const FString& UserText);

protected:
	virtual void BeginPlay() override;

private:
	void OnHttpCompleted(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
};
