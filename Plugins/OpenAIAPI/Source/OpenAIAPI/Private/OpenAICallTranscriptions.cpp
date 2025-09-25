// Copyright Kellan Mythen 2023. All rights Reserved.

#include "OpenAICallTranscriptions.h"
#include "OpenAIUtils.h"
#include "Http.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

UOpenAICallTranscriptions::UOpenAICallTranscriptions()
{
}

UOpenAICallTranscriptions::~UOpenAICallTranscriptions()
{
}

UOpenAICallTranscriptions* UOpenAICallTranscriptions::OpenAICallTranscriptions(FString fileName)
{
	UOpenAICallTranscriptions* BPNode = NewObject<UOpenAICallTranscriptions>();
	BPNode->fileName = fileName + ".wav";
	return BPNode;
}

void UOpenAICallTranscriptions::Activate()
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
	}
	
	// get the absolutePath to the wav file
	FString relativePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() + "BouncedWavFiles/" + fileName);
	FString absolutePath = FPaths::ConvertRelativePathToFull(relativePath);
	
	FString tempHeader = "Bearer ";
	tempHeader += _apiKey;
	
	// Create the HTTP request
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

	// Set the request method, URL, and headers

	HttpRequest->SetURL("https://api.openai.com/v1/audio/transcriptions");
	HttpRequest->SetVerb("POST");
	
	// Set the content type, boundary, and form data
	HttpRequest->SetHeader("Authorization", tempHeader);
	HttpRequest->SetHeader("Content-Type", "multipart/form-data; boundary=boundary");
	HttpRequest->SetHeader("model", "whisper-1");
	
	TArray<uint8> UpFileRawData;
	FFileHelper::LoadFileToArray(UpFileRawData, *absolutePath);
	
	FString boundaryStart = "\r\n--boundary\r\n";
	FString contentDispositionFile = FString::Printf(TEXT("Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"), *fileName);
	FString contentType = "Content-Type: audio/wav\r\n\r\n";
	FString boundaryMiddle = "\r\n--boundary\r\n";
	FString contentDisposition = "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
	FString transcriptionModel = "whisper-1";
	FString boundaryEnd = "\r\n--boundary--\r\n";

	TArray<uint8> data;
	data.Append((uint8*)TCHAR_TO_UTF8(*boundaryStart), boundaryStart.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*contentDispositionFile), contentDispositionFile.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*contentType), contentType.Len());
	data.Append(UpFileRawData);
	data.Append((uint8*)TCHAR_TO_UTF8(*boundaryMiddle), boundaryMiddle.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*contentDisposition), contentDisposition.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*transcriptionModel), transcriptionModel.Len());
	data.Append((uint8*)TCHAR_TO_UTF8(*boundaryEnd), boundaryEnd.Len());

	HttpRequest->SetContent(data); 

	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UOpenAICallTranscriptions::OnResponse);
	
	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Transcriptions] Failed to send HTTP request: URL=%s"), *HttpRequest->GetURL());
		Finished.Broadcast({}, TEXT("Error sending request"), false);
	}
}

void UOpenAICallTranscriptions::OnResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool WasSuccessful)
{
	const FString RequestedUrl = Request.IsValid() ? Request->GetURL() : TEXT("<invalid request>");
	const bool bResponseValid = Response.IsValid();
	const FString ResponseUrl = bResponseValid ? Response->GetURL() : TEXT("<no response>");
	const int32 StatusCode = bResponseValid ? Response->GetResponseCode() : -1;
	const FString ResponseBody = bResponseValid ? Response->GetContentAsString() : TEXT("<empty>");

	if (!WasSuccessful || !bResponseValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Transcriptions] HTTP failure (WasSuccessful=%s, Status=%d, RequestURL=%s, ResponseURL=%s) Body=%s"),
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

	UE_LOG(LogTemp, Log, TEXT("[Transcriptions] HTTP %d OK (URL=%s)"), StatusCode, *ResponseUrl);

	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		FString TextValue;
		if (JsonObject->TryGetStringField("text", TextValue))
		{
			UE_LOG(LogTemp, Log, TEXT("[Transcriptions] Extracted text: %s"), *TextValue);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Transcriptions] Missing 'text' field in JSON. Body=%s"), *ResponseBody);
			Finished.Broadcast(TEXT(""), TEXT("Failed to get 'text' field from JSON response"), false);
			return;
		}

		Finished.Broadcast(TextValue, TEXT(""), true);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Transcriptions] JSON parse failed. Body=%s"), *ResponseBody);
		Finished.Broadcast(TEXT(""), TEXT("Failed to parse JSON response"), false);
	}
}
