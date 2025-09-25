#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "DialogueNpcComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDialogueNPC, Log, All);

class FTextToSpeechBase;

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
	FString DefaultPrompt = TEXT("Hello? Connection test");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Voice")
	bool bEnableVoicePlayback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Voice")
	bool bSpeakDefaultVoiceLine = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Voice")
	FString DefaultVoiceLine = TEXT("Hi everyone");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Voice", meta=(ClampMin="0.0", ClampMax="1.0"))
	float VoiceVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Voice", meta=(ClampMin="0.0"))
	float VoicePlaybackDelaySeconds = 5.0f;

	UFUNCTION(BlueprintCallable, Category="NPC")
	void SendDefaultPrompt();

	UFUNCTION(BlueprintCallable, Category="NPC")
	void SendPrompt(const FString& UserText);

	UFUNCTION(BlueprintCallable, Category="NPC|Voice")
	void StopVoicePlayback();

	UFUNCTION(BlueprintCallable, Category="NPC|Voice")
	void SpeakLine(const FString& Line);

protected:
	virtual void BeginPlay() override;

private:
	void EnsureTextToSpeechInitialized();
	void ScheduleSpeak(const FString& Line, float DelaySeconds);

	void OnHttpCompleted(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	TSharedPtr<FTextToSpeechBase> TextToSpeech;
};
