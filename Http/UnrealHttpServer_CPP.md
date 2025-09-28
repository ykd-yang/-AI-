# Unreal HTTP Server C++ Implementation

이 문서는 `UnrealHttpServer_Blueprint.md`에 정리된 블루프린트 절차를 C++ 코드로 구현한 새 액터들을 소개합니다. 새 클래스들은 `Source/NewIdolProject/Public` 폴더에 추가되며, 레벨에 배치하여 바로 사용할 수 있습니다.

## 추가된 클래스

- `AHttpPoseServerActor` (`HttpPoseServerActor.h/cpp`)
  - HTTP Server 플러그인을 사용하여 지정한 포트(`ListenPort`)와 경로(`PoseRoute`)에서 POST 요청을 수신합니다.
  - 본문(JSON)을 파싱하여 `message`, `confidence`, `timestamp`, `client_id` 값을 추출합니다.
  - 수신한 데이터를 `AHttpPoseScoreActor`에게 전달하고, `OnPoseDataReceived` 델리게이트를 통해 블루프린트에서도 처리할 수 있습니다.
  - `TestHttpRequest()` 함수를 호출하면 문서에서 설명한 “Any Key → T” 테스트 흐름을 C++에서 재현할 수 있습니다.

- `AHttpPoseScoreActor` (`HttpPoseScoreActor.h/cpp`)
  - 텍스트 렌더 컴포넌트로 최신 메시지와 신뢰도(confidence)를 표시합니다.
  - `UpdatePoseData()`가 블루프린트의 `UpdatePoseData` 커스텀 이벤트 역할을 대체합니다.

## 사용 방법

1. `NewIdolProject.uproject`에 `HTTPServer` 플러그인이 활성화되어 있으며, `NewIdolProject.Build.cs`에는 `HTTPServer` 모듈이 의존성으로 추가되어 있습니다.
2. 레벨에 `HttpPoseServerActor`와 `HttpPoseScoreActor`를 배치합니다.
   - `HttpPoseServerActor`의 `ScoreDisplayActor` 속성에 스코어 액터를 할당하면 JSON 수신 시 자동으로 텍스트가 갱신됩니다.
3. 외부에서 `POST http://<Host>:<ListenPort>/pose_data`로 JSON을 전송합니다. 예시:

   ```json
   {
     "message": "Pose Matched",
     "confidence": 0.92,
     "timestamp": 12.34,
     "client_id": "remote_client"
   }
   ```

4. 정상 수신 시 콘솔과 온스크린 디버그 메시지로 상태가 출력되고, 텍스트 렌더가 `message` / `confidence` 값을 표시합니다.

## 블루프린트 연동

- 기존 문서에서 정의한 `ReceivePoseData`, `UpdateScore`, `TestHttpRequest` 논리는 모두 `HttpPoseServerActor`의 블루프린트 호출 가능 함수로 노출되어 있습니다.
- `OnPoseDataReceived` 델리게이트는 Blueprint에서 바인딩하여 추가적인 시각화나 로깅을 구현할 수 있습니다.

이로써 `UnrealHttpServer_Blueprint.md`에서 수행하던 작업을 C++ 기반으로 재사용 가능하며, 향후 확장(로깅, 검증, 에러 응답 등)을 코드 차원에서 쉽게 추가할 수 있습니다.

