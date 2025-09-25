// Copyright Kellan Mythen 2023. All rights Reserved.

#include "OpenAICallChat.h"
#include "OpenAIUtils.h"
#include "Http.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "OpenAIParser.h"

UOpenAICallChat::UOpenAICallChat()
{
}

UOpenAICallChat::~UOpenAICallChat()
{
}

UOpenAICallChat* UOpenAICallChat::OpenAICallChat(FChatSettings chatSettingsInput)
{
	UOpenAICallChat* BPNode = NewObject<UOpenAICallChat>();
	BPNode->chatSettings = chatSettingsInput;
	return BPNode;
}

void UOpenAICallChat::Activate()
{
	FString _apiKey;
	if (UOpenAIUtils::getUseApiKeyFromEnvironmentVars())
		_apiKey = UOpenAIUtils::GetEnvironmentVariable(TEXT("OPENAI_API_KEY"));
	else
		_apiKey = UOpenAIUtils::getApiKey();
	
	// checking parameters are valid
	if (_apiKey.IsEmpty())
	{
		Finished.Broadcast({}, TEXT("Api key is not set"), false);
	}	else
	{
	
		auto HttpRequest = FHttpModule::Get().CreateRequest();

		FString apiMethod;
		switch (chatSettings.model)
		{
		case EOAChatEngineType::GPT_3_5_TURBO:
			apiMethod = "gpt-3.5-turbo";
			break;
		case EOAChatEngineType::GPT_4:
			apiMethod = "gpt-4";
			break;
		case EOAChatEngineType::GPT_4_32k:
			apiMethod = "gpt-4-32k";
			break;
		}
		
		//TODO: add aditional params to match the ones listed in the curl response in: https://platform.openai.com/docs/api-reference/making-requests
	
		// convert parameters to strings
		FString tempHeader = "Bearer ";
		tempHeader += _apiKey;

		// set headers
		FString url = FString::Printf(TEXT("https://api.openai.com/v1/chat/completions"));
		HttpRequest->SetURL(url);
		HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		HttpRequest->SetHeader(TEXT("Authorization"), tempHeader);

		//build payload
		TSharedPtr<FJsonObject> _payloadObject = MakeShareable(new FJsonObject());
		_payloadObject->SetStringField(TEXT("model"), apiMethod);
		_payloadObject->SetNumberField(TEXT("max_tokens"), chatSettings.maxTokens);

		
		// convert role enum to model string
		if (!(chatSettings.messages.Num() == 0))
		{
			TArray<TSharedPtr<FJsonValue>> Messages;
			FString role;
			for (int i = 0; i < chatSettings.messages.Num(); i++)
			{
				TSharedPtr<FJsonObject> Message = MakeShareable(new FJsonObject());
				switch (chatSettings.messages[i].role)
				{
				case EOAChatRole::USER:
					role = "user";
					break;
				case EOAChatRole::ASSISTANT:
					role = "assistant";
					break;
				case EOAChatRole::SYSTEM:
					role = "system";
					break;
				}
				Message->SetStringField(TEXT("role"), role);
				Message->SetStringField(TEXT("content"), chatSettings.messages[i].content);
				Messages.Add(MakeShareable(new FJsonValueObject(Message)));
			}
			_payloadObject->SetArrayField(TEXT("messages"), Messages);
		}

		// convert payload to string
		FString _payload;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&_payload);
		FJsonSerializer::Serialize(_payloadObject.ToSharedRef(), Writer);

		// commit request
		HttpRequest->SetVerb(TEXT("POST"));
		HttpRequest->SetContentAsString(_payload);

		if (HttpRequest->ProcessRequest())
		{
			HttpRequest->OnProcessRequestComplete().BindUObject(this, &UOpenAICallChat::OnResponse);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Chat] Failed to send HTTP request: URL=%s"), *HttpRequest->GetURL());
			Finished.Broadcast({}, TEXT("Error sending request"), false);
		}
	}
}

void UOpenAICallChat::OnResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool WasSuccessful)
{
	const FString RequestedUrl = Request.IsValid() ? Request->GetURL() : TEXT("<invalid request>");
	const bool bResponseValid = Response.IsValid();
	const FString ResponseUrl = bResponseValid ? Response->GetURL() : TEXT("<no response>");
	const int32 StatusCode = bResponseValid ? Response->GetResponseCode() : -1;
	const FString ResponseBody = bResponseValid ? Response->GetContentAsString() : TEXT("<empty>");

	if (!WasSuccessful || !bResponseValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Chat] HTTP failure (WasSuccessful=%s, Status=%d, RequestURL=%s, ResponseURL=%s) Body=%s"),
			WasSuccessful ? TEXT("true") : TEXT("false"),
			StatusCode,
			*RequestedUrl,
			*ResponseUrl,
			*ResponseBody);
		if (Finished.IsBound())
		{
			Finished.Broadcast({}, *ResponseBody, false);
		}
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Chat] HTTP %d OK (URL=%s)"), StatusCode, *ResponseUrl);

	TSharedPtr<FJsonObject> responseObject;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (FJsonSerializer::Deserialize(reader, responseObject))
	{
		bool err = responseObject->HasField("error");

		if (err)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Chat] API error JSON: %s"), *ResponseBody);
			Finished.Broadcast({}, TEXT("Api error"), false);
			return;
		}

		OpenAIParser parser(chatSettings);
		FChatCompletion _out = parser.ParseChatCompletion(*responseObject);

		Finished.Broadcast(_out, "", true);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Chat] JSON parse failed. Body=%s"), *ResponseBody);
		if (Finished.IsBound())
		{
			Finished.Broadcast({}, TEXT("Failed to parse JSON response"), false);
		}
	}
}

