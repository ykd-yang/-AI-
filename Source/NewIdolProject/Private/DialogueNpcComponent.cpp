#include "DialogueNpcComponent.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// ⚠️ Editor 전용 헤더는 절대 include 하지 마세요 (런타임 컴포넌트에서 금지)
// #include "Editor/..."  전부 삭제!

DEFINE_LOG_CATEGORY(LogDialogueNPC);

void UDialogueNpcComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoSendOnBeginPlay)
	{
		UE_LOG(LogDialogueNPC, Log, TEXT("BeginPlay auto-send enabled. Sending default prompt."));
		SendDefaultPrompt();
	}
}

void UDialogueNpcComponent::SendDefaultPrompt()
{
	UE_LOG(LogDialogueNPC, Log, TEXT("SendDefaultPrompt: %s"), *DefaultPrompt);
	SendPrompt(DefaultPrompt);
}

void UDialogueNpcComponent::SendPrompt(const FString& UserText)
{
	TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
	Msg->SetStringField(TEXT("role"), TEXT("user"));
	Msg->SetStringField(TEXT("text"), UserText);

	TArray<TSharedPtr<FJsonValue>> Msgs;
	Msgs.Add(MakeShared<FJsonValueObject>(Msg));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("messages"), Msgs);

	FString Body;
	{
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	}

	auto& Http = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = Http.CreateRequest();
	Req->SetURL(ProxyURL);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
	Req->SetContentAsString(Body);

	UE_LOG(LogDialogueNPC, Log, TEXT("Sending prompt to %s"), *ProxyURL);
	UE_LOG(LogDialogueNPC, Verbose, TEXT("Request body: %s"), *Body);

	Req->OnProcessRequestComplete().BindUObject(this, &UDialogueNpcComponent::OnHttpCompleted);
	if (!Req->ProcessRequest())
	{
		UE_LOG(LogDialogueNPC, Warning, TEXT("ProcessRequest returned false"));
		OnNPCReply.Broadcast(TEXT("[요청 실패]"));
		return;
	}
}

void UDialogueNpcComponent::OnHttpCompleted(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
{
	const bool bResponseValid = Res.IsValid();
	if (!bOk || !bResponseValid)
	{
		UE_LOG(LogDialogueNPC, Warning, TEXT("HTTP request failed (bOk=%s, ResponseValid=%s)"),
			bOk ? TEXT("true") : TEXT("false"),
			bResponseValid ? TEXT("true") : TEXT("false"));
		OnNPCReply.Broadcast(TEXT("[네트워크 오류]"));
		return;
	}

	const int32 Code = Res->GetResponseCode();
	const FString ResponseBody = Res->GetContentAsString();
	if (Code >= 400)
	{
		UE_LOG(LogDialogueNPC, Warning, TEXT("Server returned error %d. Body=%s"), Code, *ResponseBody);
		OnNPCReply.Broadcast(FString::Printf(TEXT("[서버 오류 %d] %s"), Code, *ResponseBody));
		return;
	}

	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
	{
		FString Reply;
		if (Obj->TryGetStringField(TEXT("reply"), Reply))
		{
			UE_LOG(LogDialogueNPC, Log, TEXT("Received reply: %s"), *Reply);
			OnNPCReply.Broadcast(Reply);
			return;
		}

		TArray<FString> Keys;
		Obj->Values.GetKeys(Keys);
		UE_LOG(LogDialogueNPC, Warning, TEXT("JSON lacked 'reply'. Keys=%s"), *FString::Join(Keys, TEXT(", ")));
	}
	else
	{
		UE_LOG(LogDialogueNPC, Warning, TEXT("JSON parse failed: %s"), *ResponseBody);
	}

	OnNPCReply.Broadcast(TEXT("[파싱 실패]"));
}
