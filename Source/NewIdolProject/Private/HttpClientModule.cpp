#include "HttpClientModule.h"

#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Internationalization/UTF8ToTCHAR.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Containers/StringConv.h"

DEFINE_LOG_CATEGORY_STATIC(LogHttpClientModule, Log, All);

namespace
{
    FString ConvertResponseToString(const TArray<uint8>& Data)
    {
        if (Data.Num() == 0)
        {
            return FString();
        }

        FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Data.GetData()), Data.Num());
        return FString(Converter.Length(), Converter.Get());
    }
}

FString FHttpClientProfile::ToDebugString() const
{
    const FString JoinedItems = FString::Join(Items, TEXT(", "));
    return FString::Printf(TEXT("Name:%s | Age:%d | Height:%.2f | Items:[%s]"), *Name, Age, Height, *JoinedItems);
}

AHttpClientModule::AHttpClientModule()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AHttpClientModule::BeginPlay()
{
    Super::BeginPlay();
    SendPostRequest();
    SendGetRequest();
}

void AHttpClientModule::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
}

void AHttpClientModule::SendGetRequest()
{
    if (EndpointUrl.IsEmpty())
    {
        UE_LOG(LogHttpClientModule, Warning, TEXT("EndpointUrl is empty; GET ??? ?? ? ????."));
        return;
    }

    FString Url = EndpointUrl;
    if (!RequestName.IsEmpty())
    {
        const FString Query = FString::Printf(TEXT("?name=%s"), *FGenericPlatformHttp::UrlEncode(RequestName));
        Url += Query;
    }

    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accept-Charset"), TEXT("utf-8"));
    Request->OnProcessRequestComplete().BindUObject(this, &AHttpClientModule::HandleResponse);

    if (!Request->ProcessRequest())
    {
        UE_LOG(LogHttpClientModule, Error, TEXT("GET ?? ?? ??: %s"), *Url);
    }
}

void AHttpClientModule::SendPostRequest()
{
    if (EndpointUrl.IsEmpty())
    {
        UE_LOG(LogHttpClientModule, Warning, TEXT("EndpointUrl is empty; POST ??? ?? ? ????."));
        return;
    }

    if (RequestName.IsEmpty())
    {
        UE_LOG(LogHttpClientModule, Warning, TEXT("POST ??? ?? RequestName? ?? ????."));
        return;
    }

    const FString Payload = BuildRequestPayload();
    UE_LOG(LogHttpClientModule, Log, TEXT("POST ??: %s"), *Payload);

    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
    Request->SetURL(EndpointUrl);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accept-Charset"), TEXT("utf-8"));

    FTCHARToUTF8 PayloadUtf8(*Payload);
    TArray<uint8> PayloadBytes;
    PayloadBytes.Append(reinterpret_cast<const uint8*>(PayloadUtf8.Get()), PayloadUtf8.Length());
    Request->SetContent(PayloadBytes);
    Request->OnProcessRequestComplete().BindUObject(this, &AHttpClientModule::HandleResponse);

    if (!Request->ProcessRequest())
    {
        UE_LOG(LogHttpClientModule, Error, TEXT("POST ?? ?? ??: %s"), *EndpointUrl);
    }
}

FString AHttpClientModule::BuildRequestPayload() const
{
    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField(TEXT("name"), RequestName);

    FString Output;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
    FJsonSerializer::Serialize(JsonObject, Writer);
    return Output;
}

void AHttpClientModule::HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    const FString Verb = Request.IsValid() ? Request->GetVerb() : TEXT("UNKNOWN");
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogHttpClientModule, Error, TEXT("%s ?? ?? ?? ??."), *Verb);
        return;
    }

    const int32 StatusCode = Response->GetResponseCode();
    if (StatusCode < 200 || StatusCode >= 300)
    {
        UE_LOG(LogHttpClientModule, Error, TEXT("%s ?? ?? %d"), *Verb, StatusCode);
        return;
    }

    const FString ResponsePayload = ConvertResponseToString(Response->GetContent());
    UE_LOG(LogHttpClientModule, Log, TEXT("%s ?? ??: %s"), *Verb, *ResponsePayload);

    LatestProfiles.Reset();
    bool bHadProfiles = false;
    if (!ParseProfilesFromJson(ResponsePayload, LatestProfiles, bHadProfiles))
    {
        UE_LOG(LogHttpClientModule, Error, TEXT("%s ?? JSON ?? ??."), *Verb);
        return;
    }

    LogProfiles(Verb, LatestProfiles);

    if (Verb.Equals(TEXT("GET"), ESearchCase::IgnoreCase))
    {
        HandleGetProfiles(LatestProfiles, bHadProfiles);
    }
}

bool AHttpClientModule::ParseProfilesFromJson(const FString& Payload, TArray<FHttpClientProfile>& OutProfiles, bool& bOutHasProfiles) const
{
    bOutHasProfiles = false;

    if (Payload.IsEmpty())
    {
        UE_LOG(LogHttpClientModule, Warning, TEXT("?? ??? ?? ????."));
        return true;
    }

    TSharedPtr<FJsonObject> JsonObject;
    const TSharedRef<TJsonReader<>> ObjectReader = TJsonReaderFactory<>::Create(Payload);
    bool bParsedObject = FJsonSerializer::Deserialize(ObjectReader, JsonObject) && JsonObject.IsValid();

    if (bParsedObject)
    {
        FHttpClientProfile Profile;
        JsonObject->TryGetStringField(TEXT("name"), Profile.Name);

        double AgeNumber = 0.0;
        if (JsonObject->TryGetNumberField(TEXT("age"), AgeNumber))
        {
            Profile.Age = static_cast<int32>(AgeNumber);
        }

        double HeightNumber = 0.0;
        if (JsonObject->TryGetNumberField(TEXT("height"), HeightNumber))
        {
            Profile.Height = static_cast<float>(HeightNumber);
        }

        Profile.Items.Reset();
        const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
        if (JsonObject->TryGetArrayField(TEXT("item"), ItemsArray) && ItemsArray)
        {
            for (const TSharedPtr<FJsonValue>& Value : *ItemsArray)
            {
                FString ItemString;
                if (Value.IsValid() && Value->TryGetString(ItemString))
                {
                    Profile.Items.Add(ItemString);
                }
            }
        }

        OutProfiles.Add(Profile);
        bOutHasProfiles = true;
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> JsonArray;
    const TSharedRef<TJsonReader<>> ArrayReader = TJsonReaderFactory<>::Create(Payload);
    if (!FJsonSerializer::Deserialize(ArrayReader, JsonArray))
    {
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Entry : JsonArray)
    {
        if (!Entry.IsValid())
        {
            continue;
        }

        const TSharedPtr<FJsonObject> EntryObject = Entry->AsObject();
        if (!EntryObject.IsValid())
        {
            continue;
        }

        FHttpClientProfile Profile;
        EntryObject->TryGetStringField(TEXT("name"), Profile.Name);

        double AgeNumber = 0.0;
        if (EntryObject->TryGetNumberField(TEXT("age"), AgeNumber))
        {
            Profile.Age = static_cast<int32>(AgeNumber);
        }

        double HeightNumber = 0.0;
        if (EntryObject->TryGetNumberField(TEXT("height"), HeightNumber))
        {
            Profile.Height = static_cast<float>(HeightNumber);
        }

        Profile.Items.Reset();
        const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
        if (EntryObject->TryGetArrayField(TEXT("item"), ItemsArray) && ItemsArray)
        {
            for (const TSharedPtr<FJsonValue>& Value : *ItemsArray)
            {
                FString ItemString;
                if (Value.IsValid() && Value->TryGetString(ItemString))
                {
                    Profile.Items.Add(ItemString);
                }
            }
        }

        OutProfiles.Add(Profile);
    }

    bOutHasProfiles = OutProfiles.Num() > 0;
    return true;
}

void AHttpClientModule::LogProfiles(const FString& Context, const TArray<FHttpClientProfile>& Profiles) const
{
    if (Profiles.Num() == 0)
    {
        UE_LOG(LogHttpClientModule, Warning, TEXT("%s ???? ???? ?? ?????."), *Context);
        return;
    }

    for (const FHttpClientProfile& Profile : Profiles)
    {
        UE_LOG(LogHttpClientModule, Log, TEXT("%s ?? ??? -> %s"), *Context, *Profile.ToDebugString());
    }
}

void AHttpClientModule::HandleGetProfiles(const TArray<FHttpClientProfile>& Profiles, bool bHadProfiles)
{
    if (bHadProfiles && Profiles.Num() > 0)
    {
        const FHttpClientProfile& FirstProfile = Profiles[0];
        if (!FirstProfile.Name.IsEmpty() && !FirstProfile.Name.Equals(RequestName))
        {
            RequestName = FirstProfile.Name;
            UE_LOG(LogHttpClientModule, Log, TEXT("GET ???? RequestName? '%s'? ??????."), *RequestName);
            SendPostRequest();
            return;
        }
    }

    if (!RequestName.IsEmpty())
    {
        UE_LOG(LogHttpClientModule, Log, TEXT("GET ? RequestName='%s'? ?????."), *RequestName);
    }
    else
    {
        UE_LOG(LogHttpClientModule, Warning, TEXT("GET ???? ??? ???? ?? RequestName? ?? ????."));
    }
}
