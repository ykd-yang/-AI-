# 언리얼 엔진 HTTP 서버 설정 가이드

## 1. 언리얼 엔진에서 HTTP 서버 설정

### A. Blueprint로 HTTP 서버 생성

1. **새로운 Blueprint 클래스 생성**
   - Content Browser에서 우클릭 → Blueprint Class
   - Actor를 부모 클래스로 선택
   - 이름: `BP_HttpServer`

2. **HTTP 서버 컴포넌트 추가**
   ```
   Event Graph에서:
   - Begin Play 이벤트 추가
   - HTTP Request 노드 추가 (Web Request 컴포넌트)
   - 또는 C++로 HTTP 서버 구현
   ```

### B. C++로 HTTP 서버 구현 (권장)

1. **새로운 C++ 클래스 생성**
   ```cpp
   // HttpServerActor.h
   #pragma once
   
   #include "CoreMinimal.h"
   #include "GameFramework/Actor.h"
   #include "Http.h"
   #include "HttpServerActor.generated.h"
   
   UCLASS()
   class YOURPROJECT_API AHttpServerActor : public AActor
   {
       GENERATED_BODY()
       
   public:    
       AHttpServerActor();
   
   protected:
       virtual void BeginPlay() override;
   
   public:    
       virtual void Tick(float DeltaTime) override;
   
       // HTTP 요청 처리 함수
       UFUNCTION(BlueprintCallable)
       void HandlePoseData(const FString& JsonData);
       
   private:
       // HTTP 서버 설정
       void SetupHttpServer();
       void StartHttpServer();
       
       // 포즈 데이터 처리
       void ProcessPoseData(const TSharedPtr<FJsonObject> JsonObject);
       
       // BP_Score 업데이트
       UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
       class ABP_Score_C* ScoreActor;
       
       // HTTP 서버 관련
       TSharedPtr<class FHttpServer> HttpServer;
       int32 ServerPort = 4000;
   };
   ```

2. **HTTP 서버 구현**
   ```cpp
   // HttpServerActor.cpp
   #include "HttpServerActor.h"
   #include "Dom/JsonObject.h"
   #include "Serialization/JsonSerializer.h"
   #include "Serialization/JsonWriter.h"
   #include "Engine/Engine.h"
   #include "GameFramework/Actor.h"
   
   AHttpServerActor::AHttpServerActor()
   {
       PrimaryActorTick.bCanEverTick = true;
   }
   
   void AHttpServerActor::BeginPlay()
   {
       Super::BeginPlay();
       SetupHttpServer();
   }
   
   void AHttpServerActor::SetupHttpServer()
   {
       // HTTP 서버 라우트 설정
       HttpServer = FHttpServerModule::Get().CreateServer();
       
       // /pose_data 엔드포인트 등록
       HttpServer->OnHttpRequest().BindLambda([this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
       {
           if (Request.GetPath() == TEXT("/pose_data") && Request.GetVerb() == TEXT("POST"))
           {
               FString JsonString = Request.GetBodyAsString();
               HandlePoseData(JsonString);
               
               // 성공 응답
               TSharedRef<FHttpServerResponse> Response = MakeShared<FHttpServerResponse>();
               Response->Code = 200;
               Response->Headers.Add(TEXT("Content-Type"), TEXT("application/json"));
               Response->Body = TEXT("{\"status\":\"success\"}");
               
               OnComplete(Response);
               return true;
           }
           return false;
       });
       
       StartHttpServer();
   }
   
   void AHttpServerActor::StartHttpServer()
   {
       if (HttpServer.IsValid())
       {
           HttpServer->StartServer(ServerPort);
           UE_LOG(LogTemp, Warning, TEXT("HTTP Server started on port %d"), ServerPort);
       }
   }
   
   void AHttpServerActor::HandlePoseData(const FString& JsonData)
   {
       UE_LOG(LogTemp, Warning, TEXT("Received pose data: %s"), *JsonData);
       
       TSharedPtr<FJsonObject> JsonObject;
       TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
       
       if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
       {
           ProcessPoseData(JsonObject);
       }
   }
   
   void AHttpServerActor::ProcessPoseData(const TSharedPtr<FJsonObject> JsonObject)
   {
       // JSON 데이터에서 필요한 정보 추출
       FString Message = JsonObject->GetStringField(TEXT("message"));
       float Confidence = JsonObject->GetNumberField(TEXT("confidence"));
       
       // BP_Score 액터 업데이트
       if (ScoreActor)
       {
           // Blueprint 함수 호출로 텍스트 업데이트
           // ScoreActor->UpdateScore(Message, Confidence);
       }
       
       UE_LOG(LogTemp, Warning, TEXT("Processed: %s, Confidence: %f"), *Message, Confidence);
   }
   ```

### C. Blueprint에서 HTTP 서버 사용

1. **BP_HttpServer Blueprint 생성**
   - 위의 C++ 클래스를 부모로 하는 Blueprint 생성
   - ScoreActor 변수에 BP_Score 액터 할당

2. **맵에 HTTP 서버 배치**
   - 레벨에 BP_HttpServer 액터 배치
   - ScoreActor 변수에 BP_Score_C_1 할당

## 2. BP_Score Blueprint 수정

### A. 포즈 데이터를 받는 함수 추가

```cpp
// BP_Score에 추가할 함수들
UFUNCTION(BlueprintCallable)
void UpdateScore(const FString& Message, float Confidence);

UFUNCTION(BlueprintCallable)
void UpdatePoseData(const TArray<FVector>& Points);
```

### B. Blueprint Event Graph 설정

1. **Custom Event 생성**
   - Event Name: `UpdatePoseData`
   - Parameters: Message (String), Confidence (Float)

2. **Text Render 업데이트**
   ```
   UpdatePoseData Event → 
   String Concatenate (Message + " Confidence: " + Confidence.ToString()) →
   Set Text (TextRender Component)
   ```

## 3. 연결 테스트

### A. Python 클라이언트 실행

```bash
# 기본 설정으로 실행 (포트 4000 사용)
python UnrealHttpClient.py --unreal-port 4000

# 또는 다른 포트 사용
python UnrealHttpClient.py --unreal-host 127.0.0.1 --unreal-port 8080
```

### B. 언리얼 엔진에서 확인

1. **Output Log 확인**
   - Window → Developer Tools → Output Log
   - HTTP 서버 시작 메시지와 데이터 수신 로그 확인

2. **게임 내 시각적 확인**
   - BP_Score의 텍스트가 업데이트되는지 확인
   - 5초마다 텍스트가 변경되는지 확인

## 4. 문제 해결

### A. 연결 실패 시
1. **포트 확인**: 언리얼과 Python 클라이언트의 포트가 일치하는지 확인
2. **방화벽 설정**: Windows 방화벽에서 포트 허용 확인
3. **IP 주소 확인**: localhost(127.0.0.1) 사용 확인

### B. 데이터 수신 실패 시
1. **JSON 형식 확인**: response.json 파일이 올바른 형식인지 확인
2. **엔드포인트 확인**: `/pose_data` 경로가 정확한지 확인
3. **HTTP 메서드 확인**: POST 요청인지 확인

## 5. 고급 설정

### A. 보안 설정
```cpp
// HTTPS 사용 시
HttpServer->SetSecure(true);
HttpServer->SetCertificate(...);
```

### B. 다중 클라이언트 지원
```cpp
// 클라이언트 ID별 데이터 처리
FString ClientId = JsonObject->GetStringField(TEXT("client_id"));
```

### C. 실시간 데이터 처리
```cpp
// 틱마다 데이터 처리
void AHttpServerActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 실시간 포즈 데이터 처리 로직
}
```

## 6. 완성된 시스템 구조

```
Python UnrealHttpClient.py (5초마다)
    ↓ HTTP POST /pose_data
Unreal Engine HTTP Server (포트 4000)
    ↓ 데이터 파싱 및 처리
BP_Score Actor (텍스트 업데이트)
    ↓ 시각적 표시
Game World (실시간 포즈 정보 표시)
```

이 설정을 통해 Python에서 언리얼 엔진으로 실시간 포즈 데이터를 전송하고, 게임 내에서 시각적으로 확인할 수 있습니다.
