// Copyright Kellan Mythen 2023. All rights Reserved.


#include "OpenAICallDALLE.h"
#include "OpenAIUtils.h"
#include "Http.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "OpenAIParser.h"


UOpenAICallDALLE::UOpenAICallDALLE()
{
}

UOpenAICallDALLE::~UOpenAICallDALLE()
{
}

UOpenAICallDALLE* UOpenAICallDALLE::OpenAICallDALLE(EOAImageSize imageSizeInput, FString promptInput, int32 numImagesInput)
{
	UOpenAICallDALLE* BPNode = NewObject<UOpenAICallDALLE>();
	BPNode->imageSize = imageSizeInput;
	BPNode->prompt = promptInput;
	BPNode->numImages = numImagesInput;
	return BPNode;
}

void UOpenAICallDALLE::Activate()
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
	} else if (prompt.IsEmpty())
	{
		Finished.Broadcast({}, TEXT("Prompt is empty"), false);
	} else if (numImages < 1 || numImages > 10)
	{
		Finished.Broadcast({}, TEXT("NumImages must be set to a value between 1 and 10"), false);
	}
	
	auto HttpRequest = FHttpModule::Get().CreateRequest();
	
	FString imageResolution;
	switch (imageSize)
	{
	case EOAImageSize::SMALL:
			imageResolution = "256x256";
	break;
	case EOAImageSize::MEDIUM:
			imageResolution = "512x512";
	break;
	case EOAImageSize::LARGE:
			imageResolution = "1024x1024";
	break;
	}

	// convert parameters to strings
	FString tempHeader = "Bearer ";
	tempHeader += _apiKey;

	// set headers
	FString url = FString::Printf(TEXT("https://api.openai.com/v1/images/generations"));
	HttpRequest->SetURL(url);
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Authorization"), tempHeader);

	// build payload
	TSharedPtr<FJsonObject> _payloadObject = MakeShareable(new FJsonObject());
	_payloadObject->SetStringField(TEXT("prompt"), prompt);
	_payloadObject->SetNumberField(TEXT("n"), numImages);
	_payloadObject->SetStringField(TEXT("size"), imageResolution);

	// convert payload to string
	FString _payload;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&_payload);
	FJsonSerializer::Serialize(_payloadObject.ToSharedRef(), Writer);

	// commit request
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetContentAsString(_payload);

	if (HttpRequest->ProcessRequest())
	{
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UOpenAICallDALLE::OnResponse);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[DALLE] Failed to send HTTP request: URL=%s"), *HttpRequest->GetURL());
		Finished.Broadcast({}, TEXT("Error sending request"), false);
	}
}

void UOpenAICallDALLE::OnResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool WasSuccessful)
{
	const FString RequestedUrl = Request.IsValid() ? Request->GetURL() : TEXT("<invalid request>");
	const bool bResponseValid = Response.IsValid();
	const FString ResponseUrl = bResponseValid ? Response->GetURL() : TEXT("<no response>");
	const int32 StatusCode = bResponseValid ? Response->GetResponseCode() : -1;
	const FString ResponseBody = bResponseValid ? Response->GetContentAsString() : TEXT("<empty>");

	if (!WasSuccessful || !bResponseValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DALLE] HTTP failure (WasSuccessful=%s, Status=%d, RequestURL=%s, ResponseURL=%s) Body=%s"),
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

	UE_LOG(LogTemp, Log, TEXT("[DALLE] HTTP %d OK (URL=%s)"), StatusCode, *ResponseUrl);

	TSharedPtr<FJsonObject> responseObject;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (FJsonSerializer::Deserialize(reader, responseObject))
	{
		bool err = responseObject->HasField("error");
		
		if (err)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DALLE] API error JSON: %s"), *ResponseBody);
			Finished.Broadcast({}, TEXT("Api error"), false);
			return;
		}

		OpenAIParser parser(settings);
		TArray<FString> _out;

		auto GeneratedImagesObject = responseObject->GetArrayField(TEXT("data"));
		for (auto& elem : GeneratedImagesObject)
		{
			_out.Add(parser.ParseGeneratedImage(*elem->AsObject()));
		}

		Finished.Broadcast(_out, "", true);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[DALLE] JSON parse failed. Body=%s"), *ResponseBody);
		if (Finished.IsBound())
		{
			Finished.Broadcast({}, TEXT("Failed to parse JSON response"), false);
		}
	}
}

