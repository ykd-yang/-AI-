#include "DialogueNpcComponent.h"







#include "Dom/JsonObject.h"



#include "GenericPlatform/ITextToSpeechFactory.h"



#include "HttpModule.h"



#include "Interfaces/IHttpResponse.h"



#include "Modules/ModuleManager.h"



#include "Serialization/JsonSerializer.h"



#include "Serialization/JsonWriter.h"



#include "TextToSpeech.h"



#include "TimerManager.h"







DEFINE_LOG_CATEGORY(LogDialogueNPC);







void UDialogueNpcComponent::BeginPlay()



{



    Super::BeginPlay();







    EnsureTextToSpeechInitialized();







    if (bAutoSendOnBeginPlay)



    {



        UE_LOG(LogDialogueNPC, Log, TEXT("BeginPlay auto-send enabled. Sending default prompt."));







        if (bEnableVoicePlayback && !DefaultVoiceLine.IsEmpty())



        {



            ScheduleSpeak(DefaultVoiceLine, VoicePlaybackDelaySeconds);



        }







        SendDefaultPrompt();



    }



}







void UDialogueNpcComponent::EnsureTextToSpeechInitialized()



{



    if (TextToSpeech.IsValid() || !bEnableVoicePlayback)



    {



        return;



    }







    if (!FModuleManager::Get().IsModuleLoaded(TEXT("TextToSpeech")))



    {



        FModuleManager::Get().LoadModule(TEXT("TextToSpeech"));



    }







    if (!FModuleManager::Get().IsModuleLoaded(TEXT("TextToSpeech")))



    {



        UE_LOG(LogDialogueNPC, Warning, TEXT("TextToSpeech module could not be loaded."));



        return;



    }







    const TSharedPtr<ITextToSpeechFactory> Factory = ITextToSpeechModule::Get().GetPlatformFactory();



    if (!Factory.IsValid())



    {



        UE_LOG(LogDialogueNPC, Warning, TEXT("TextToSpeech factory is not available."));



        return;



    }







    TextToSpeech = Factory->Create();



    if (!TextToSpeech.IsValid())



    {



        UE_LOG(LogDialogueNPC, Warning, TEXT("Failed to create TextToSpeech instance."));



        return;



    }







    TextToSpeech->SetVolume(VoiceVolume);



    TextToSpeech->Activate();



    UE_LOG(LogDialogueNPC, Log, TEXT("Text-to-speech initialized."));



}







void UDialogueNpcComponent::ScheduleSpeak(const FString& Line, float DelaySeconds)



{



    if (!bEnableVoicePlayback || Line.IsEmpty())



    {



        return;



    }







    UWorld* World = GetWorld();



    if (!World)



    {



        SpeakLine(Line);



        return;



    }







    if (DelaySeconds <= 0.f)



    {



        SpeakLine(Line);



        return;



    }







    const TWeakObjectPtr<UDialogueNpcComponent> WeakThis(this);



    FTimerDelegate Delegate = FTimerDelegate::CreateLambda([WeakThis, Line]()



    {



        if (WeakThis.IsValid())



        {



            WeakThis->SpeakLine(Line);



        }



    });







    FTimerHandle TimerHandle;



    World->GetTimerManager().SetTimer(TimerHandle, Delegate, DelaySeconds, false);



}







void UDialogueNpcComponent::SendDefaultPrompt()



{



    UE_LOG(LogDialogueNPC, Log, TEXT("SendDefaultPrompt: %s"), *DefaultPrompt);



    SendPrompt(DefaultPrompt);



}







void UDialogueNpcComponent::SpeakLine(const FString& Line)



{



    if (!bEnableVoicePlayback || Line.IsEmpty())



    {



        return;



    }







    EnsureTextToSpeechInitialized();



    if (!TextToSpeech.IsValid())



    {



        UE_LOG(LogDialogueNPC, Warning, TEXT("TextToSpeech instance not available for speech."));



        return;



    }







    TextToSpeech->SetVolume(VoiceVolume);



    if (TextToSpeech->IsSpeaking())



    {



        UE_LOG(LogDialogueNPC, Verbose, TEXT("Interrupting previous speech."));



        TextToSpeech->StopSpeaking();



    }







    UE_LOG(LogDialogueNPC, Log, TEXT("TTS Speak: %s"), *Line);



    TextToSpeech->Speak(Line);



}







void UDialogueNpcComponent::StopVoicePlayback()



{



    if (TextToSpeech.IsValid() && TextToSpeech->IsSpeaking())



    {



        UE_LOG(LogDialogueNPC, Verbose, TEXT("Stopping current TTS playback."));



        TextToSpeech->StopSpeaking();



    }



}







void UDialogueNpcComponent::SendPrompt(const FString& UserText)



{



    TSharedPtr<FJsonObject> MessageObject = MakeShared<FJsonObject>();



    MessageObject->SetStringField(TEXT("role"), TEXT("user"));



    MessageObject->SetStringField(TEXT("text"), UserText);







    TArray<TSharedPtr<FJsonValue>> Messages;



    Messages.Add(MakeShared<FJsonValueObject>(MessageObject));







    TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();



    RootObject->SetArrayField(TEXT("messages"), Messages);







    FString Body;



    {



        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);



        FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);



    }







    auto& HttpModule = FHttpModule::Get();



    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();



    Request->SetURL(ProxyURL);



    Request->SetVerb(TEXT("POST"));



    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));



    Request->SetContentAsString(Body);







    UE_LOG(LogDialogueNPC, Log, TEXT("Sending prompt to %s"), *ProxyURL);



    UE_LOG(LogDialogueNPC, Verbose, TEXT("Request body: %s"), *Body);







    Request->OnProcessRequestComplete().BindUObject(this, &UDialogueNpcComponent::OnHttpCompleted);



    if (!Request->ProcessRequest())



    {



        UE_LOG(LogDialogueNPC, Warning, TEXT("ProcessRequest returned false."));



        OnNPCReply.Broadcast(TEXT("[요청 실패]"));



        return;



    }



}







void UDialogueNpcComponent::OnHttpCompleted(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bOk)



{



    const bool bResponseValid = Response.IsValid();



    if (!bOk || !bResponseValid)



    {



        UE_LOG(LogDialogueNPC, Warning, TEXT("HTTP request failed (bOk=%s, ResponseValid=%s)"),



               bOk ? TEXT("true") : TEXT("false"),



               bResponseValid ? TEXT("true") : TEXT("false"));



        OnNPCReply.Broadcast(TEXT("[네트워크 오류]"));



        return;



    }







    const int32 StatusCode = Response->GetResponseCode();



    const FString ResponseBody = Response->GetContentAsString();



    if (StatusCode >= 400)



    {



        UE_LOG(LogDialogueNPC, Warning, TEXT("Server returned error %d. Body=%s"), StatusCode, *ResponseBody);



        OnNPCReply.Broadcast(FString::Printf(TEXT("[서버 오류 %d] %s"), StatusCode, *ResponseBody));



        return;



    }







    TSharedPtr<FJsonObject> ResponseJson;



    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);



    if (FJsonSerializer::Deserialize(Reader, ResponseJson) && ResponseJson.IsValid())



    {



        FString Reply;



        if (ResponseJson->TryGetStringField(TEXT("reply"), Reply))



        {



            UE_LOG(LogDialogueNPC, Log, TEXT("Received reply: %s"), *Reply);



            OnNPCReply.Broadcast(Reply);



            ScheduleSpeak(Reply, VoicePlaybackDelaySeconds);



            return;



        }







        TArray<FString> Keys;



        ResponseJson->Values.GetKeys(Keys);



        UE_LOG(LogDialogueNPC, Warning, TEXT("JSON lacked 'reply'. Keys=%s"), *FString::Join(Keys, TEXT(", ")));



    }



    else



    {



        UE_LOG(LogDialogueNPC, Warning, TEXT("JSON parse failed: %s"), *ResponseBody);



    }







    OnNPCReply.Broadcast(TEXT("[파싱 실패]"));



}
