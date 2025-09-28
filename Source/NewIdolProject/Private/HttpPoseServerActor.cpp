#include "HttpPoseServerActor.h"

#include "HttpPoseScoreActor.h"

#include "Async/Async.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HttpPath.h"
#include "HttpRequestHandler.h"
#include "HttpResultCallback.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "Internationalization/UTF8ToTCHAR.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogHttpPoseServer);

namespace
{
	FString BytesToUtf8String(const TArray<uint8>& Bytes)
	{
		if (Bytes.Num() == 0)
		{
			return FString();
		}

		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		return FString(Converter.Length(), Converter.Get());
	}
}

AHttpPoseServerActor::AHttpPoseServerActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusText->SetupAttachment(Root);
	StatusText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	StatusText->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	StatusText->SetWorldSize(36.f);
	StatusText->SetText(FText::FromString(TEXT("HTTP Server Idle")));
}

void AHttpPoseServerActor::BeginPlay()
{
	Super::BeginPlay();

	ApplyStatusText(TEXT("HTTP Server Initializing..."));
	LogOnScreen(TEXT("HTTP Server Starting..."));

	if (bAutoStart)
	{
		if (StartHttpServer())
		{
			FTimerHandle ReadyTimerHandle;
			GetWorldTimerManager().SetTimer(ReadyTimerHandle, [this]()
			{
				const FString RouteDisplay = RouteForDisplay();
				ApplyStatusText(FString::Printf(TEXT("Listening on %d%s"), ListenPort, *RouteDisplay));
				LogOnScreen(FString::Printf(TEXT("HTTP Server Ready on Port %d"), ListenPort), FColor::Green);
			}, 2.0f, false);
		}
		else
		{
			ApplyStatusText(TEXT("Failed to start HTTP Server"));
		}
	}
}

void AHttpPoseServerActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopHttpServer();
	Super::EndPlay(EndPlayReason);
}

bool AHttpPoseServerActor::StartHttpServer()
{
	if (bServerActive)
	{
		return true;
	}

	FHttpServerModule& HttpModule = FHttpServerModule::Get();
	HttpRouter = HttpModule.GetHttpRouter(static_cast<uint32>(ListenPort), true);
	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogHttpPoseServer, Error, TEXT("Failed to acquire HTTP router for port %d"), ListenPort);
		return false;
	}

	PoseRouteHandle = HttpRouter->BindRoute(FHttpPath(PoseRoute), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &AHttpPoseServerActor::HandlePoseDataRequest));

	if (!PoseRouteHandle.IsValid())
	{
		UE_LOG(LogHttpPoseServer, Error, TEXT("Failed to bind HTTP route %s"), *PoseRoute);
		HttpRouter.Reset();
		return false;
	}

	HttpModule.StartAllListeners();
	bServerActive = true;
	const FString RouteDisplay = RouteForDisplay();
	ApplyStatusText(FString::Printf(TEXT("Listening on %d%s"), ListenPort, *RouteDisplay));
	UE_LOG(LogHttpPoseServer, Log, TEXT("HTTP Server listening on port %d route %s"), ListenPort, *RouteDisplay);
	return true;
}

void AHttpPoseServerActor::StopHttpServer()
{
	if (HttpRouter.IsValid() && PoseRouteHandle.IsValid())
	{
		HttpRouter->UnbindRoute(PoseRouteHandle);
		PoseRouteHandle.Reset();
	}

	if (bServerActive)
	{
		FHttpServerModule::Get().StopAllListeners();
		bServerActive = false;
	}

	HttpRouter.Reset();
	ApplyStatusText(TEXT("HTTP Server Stopped"));
}

void AHttpPoseServerActor::ApplyStatusText(const FString& InStatus) const
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(InStatus));
	}
}

void AHttpPoseServerActor::LogOnScreen(const FString& Message, const FColor& Colour) const
{
	if (bShowDebugMessages && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this) + 1, 5.f, Colour, Message);
	}
}

void AHttpPoseServerActor::ReceivePoseData(const FString& JsonData, const FString& ClientId, float Timestamp)
{
	UE_LOG(LogHttpPoseServer, Log, TEXT("Received pose payload from %s (timestamp %.3f): %s"), *ClientId, Timestamp, *JsonData);
	LogOnScreen(FString::Printf(TEXT("Received pose data from %s"), *ClientId));
}

void AHttpPoseServerActor::UpdateScore(const FString& Message, double Confidence)
{
	EnsureScoreActorReference();
	if (ScoreDisplayActor)
	{
		ScoreDisplayActor->UpdatePoseData(Message, Confidence);
	}
	else
	{
		UE_LOG(LogHttpPoseServer, Warning, TEXT("No score display actor assigned to update."));
	}
}

void AHttpPoseServerActor::TestHttpRequest()
{
	TSharedPtr<FJsonObject> TestPayload = MakeShared<FJsonObject>();
	TestPayload->SetStringField(TEXT("message"), TEXT("Test Message"));
	TestPayload->SetNumberField(TEXT("confidence"), 0.85);
	TestPayload->SetNumberField(TEXT("timestamp"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
	TestPayload->SetStringField(TEXT("client_id"), TEXT("test_client"));

	FString PayloadString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadString);
	FJsonSerializer::Serialize(TestPayload.ToSharedRef(), Writer);

    FPoseDataPayload Payload;
    FString Error;
    if (ParsePoseData(PayloadString, Payload, Error))
    {
        DispatchPosePayload(PayloadString, Payload);
    }
    else
    {
        UE_LOG(LogHttpPoseServer, Warning, TEXT("Test payload failed validation: %s"), *Error);
    }
}

bool AHttpPoseServerActor::HandlePoseDataRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	const FString JsonPayload = BytesToUtf8String(Request.Body);
	FPoseDataPayload Payload;
	FString ErrorMessage;

	if (!ParsePoseData(JsonPayload, Payload, ErrorMessage))
	{
		UE_LOG(LogHttpPoseServer, Warning, TEXT("Invalid pose payload: %s"), *ErrorMessage);
		const FString ResponseJson = BuildResponseJson(Payload, false, ErrorMessage);
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseJson, TEXT("application/json; charset=utf-8"));
		Response->Code = EHttpServerResponseCodes::BadRequest;
		Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });
		OnComplete(MoveTemp(Response));
		return true;
	}

	const FString ResponseJson = BuildResponseJson(Payload, true, TEXT(""));
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseJson, TEXT("application/json; charset=utf-8"));
	Response->Code = EHttpServerResponseCodes::Ok;
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("*") });

    DispatchPosePayload(JsonPayload, Payload);
    OnComplete(MoveTemp(Response));
    return true;
}

FString AHttpPoseServerActor::BuildResponseJson(const FPoseDataPayload& Payload, bool bSuccess, const FString& ErrorMessage) const
{
	TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
	ResponseObject->SetStringField(TEXT("status"), bSuccess ? TEXT("ok") : TEXT("error"));
	ResponseObject->SetStringField(TEXT("client_id"), Payload.ClientId);
	ResponseObject->SetNumberField(TEXT("timestamp"), Payload.Timestamp);
	if (!bSuccess)
	{
		ResponseObject->SetStringField(TEXT("message"), ErrorMessage);
	}

	FString ResponseString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
	FJsonSerializer::Serialize(ResponseObject.ToSharedRef(), Writer);
	return ResponseString;
}

bool AHttpPoseServerActor::ParsePoseData(const FString& JsonPayload, FPoseDataPayload& OutPayload, FString& OutError) const
{
	OutPayload = FPoseDataPayload();

	TSharedPtr<FJsonObject> PayloadObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);
	if (!FJsonSerializer::Deserialize(Reader, PayloadObject) || !PayloadObject.IsValid())
	{
		OutError = TEXT("Failed to parse JSON payload");
		return false;
	}

	if (!PayloadObject->TryGetStringField(TEXT("message"), OutPayload.Message))
	{
		OutError = TEXT("Missing 'message' field");
		return false;
	}

	PayloadObject->TryGetStringField(TEXT("client_id"), OutPayload.ClientId);
	if (OutPayload.ClientId.IsEmpty())
	{
		OutPayload.ClientId = TEXT("anonymous");
	}

	if (!PayloadObject->TryGetNumberField(TEXT("confidence"), OutPayload.Confidence))
	{
		OutPayload.Confidence = 0.0;
	}

	if (!PayloadObject->TryGetNumberField(TEXT("timestamp"), OutPayload.Timestamp))
	{
		OutPayload.Timestamp = FPlatformTime::Seconds();
	}

	OutError.Empty();
	return true;
}

void AHttpPoseServerActor::DispatchPosePayload(const FString& JsonPayload, const FPoseDataPayload& Payload)
{
    AsyncTask(ENamedThreads::GameThread, [this, JsonPayload, Payload]()
    {
        HandlePosePayloadOnGameThread(JsonPayload, Payload);
    });
}

void AHttpPoseServerActor::HandlePosePayloadOnGameThread(const FString& JsonPayload, const FPoseDataPayload Payload)
{
	ReceivePoseData(JsonPayload, Payload.ClientId, static_cast<float>(Payload.Timestamp));
	UpdateScore(Payload.Message, Payload.Confidence);
	OnPoseDataReceived.Broadcast(Payload);
}

void AHttpPoseServerActor::EnsureScoreActorReference()
{
    if (ScoreDisplayActor && ScoreDisplayActor->IsPendingKill())
    {
        ScoreDisplayActor = nullptr;
    }

    if (!ScoreDisplayActor)
    {
        UWorld* World = GetWorld();
        if (!World)
        {
            return;
        }

		for (TActorIterator<AHttpPoseScoreActor> It(World); It; ++It)
		{
			ScoreDisplayActor = *It;
			break;
        }
    }
}
FString AHttpPoseServerActor::RouteForDisplay() const
{
	if (PoseRoute.StartsWith(TEXT("/")))
	{
		return PoseRoute;
	}

	return FString::Printf(TEXT("/%s"), *PoseRoute);
}
