# 언리얼 엔진 Blueprint HTTP 서버 설정

## 간단한 Blueprint 설정 방법

### 1. BP_HttpServer Blueprint 생성

#### A. Blueprint 클래스 생성
1. Content Browser → 우클릭 → Blueprint Class
2. Actor 선택
3. 이름: `BP_HttpServer`

#### B. 필요한 컴포넌트 추가
```
Components 패널에서:
- Scene Component (Root Component)
- Text Render Component (디버그용)
```

### 2. Event Graph 설정

#### A. Begin Play 이벤트
```
Begin Play
    ↓
Print String: "HTTP Server Starting..."
    ↓
Delay (2.0초)
    ↓
Print String: "HTTP Server Ready on Port 4000"
```

#### B. Custom Event: "ReceivePoseData"
```
Parameters:
- JsonData (String)
- ClientId (String)
- Timestamp (Float)

Actions:
1. Print String: "Received: " + JsonData
2. Parse JSON String → JsonData
3. Extract Values:
   - Message (String)
   - Confidence (Float)
4. Update BP_Score Actor
5. Print String: "Updated Score: " + Message
```

#### C. Custom Event: "UpdateScore"
```
Parameters:
- Message (String)
- Confidence (Float)

Actions:
1. Find Actor of Class (BP_Score_C)
2. Cast to BP_Score_C
3. Call Custom Event on BP_Score: "UpdatePoseData"
   - Pass Message and Confidence
```

### 3. BP_Score Blueprint 수정

#### A. Custom Event: "UpdatePoseData" 추가
```
Parameters:
- Message (String)
- Confidence (Float)

Actions:
1. String Concatenate:
   - Message + "\\nConfidence: " + Float to String (Confidence)
2. Set Text Render Component Text
3. Print String: "Score Updated"
```

### 4. HTTP 요청 시뮬레이션 (테스트용)

#### A. BP_HttpServer에 테스트 함수 추가
```
Custom Event: "TestHttpRequest"
Actions:
1. Create JSON Object
2. Set Fields:
   - "message": "Test Message"
   - "confidence": 0.85
   - "timestamp": Get Game Time in Seconds
   - "client_id": "test_client"
3. Convert to String
4. Call ReceivePoseData with JSON String
```

#### B. 키보드 입력으로 테스트
```
Event: Any Key
    ↓
Branch: Key == T
    ↓
Call TestHttpRequest
```

### 5. 실제 HTTP 서버 구현 (플러그인 필요)

#### A. HTTP Server Plugin 설치
1. Edit → Plugins
2. "HTTP Server" 검색 및 설치
3. 프로젝트 재시작

#### B. HTTP Server 컴포넌트 추가
```
Components:
- HTTP Server Component
- Server Port: 4000
- Auto Start: True
```

#### C. HTTP 요청 처리
```
HTTP Server Component Events:
- On HTTP Request
    ↓
Branch: Request Path == "/pose_data"
    ↓
Branch: Request Method == "POST"
    ↓
Get Request Body as String
    ↓
Call ReceivePoseData with Body String
    ↓
Create HTTP Response (Status: 200)
```

## 6. 연결 테스트 단계

### A. 언리얼 엔진에서 준비
1. BP_HttpServer를 레벨에 배치
2. BP_Score를 레벨에 배치
3. Play 버튼 클릭
4. Output Log에서 "HTTP Server Ready" 메시지 확인

### B. Python 클라이언트 실행
```bash
# 터미널에서 실행
python UnrealHttpClient.py --unreal-port 4000
```

### C. 결과 확인
1. 언리얼 엔진 Output Log에서 데이터 수신 확인
2. BP_Score의 텍스트가 5초마다 업데이트되는지 확인
3. Python 콘솔에서 전송 성공 메시지 확인

## 7. 문제 해결 체크리스트

### A. 연결 문제
- [ ] 언리얼 엔진이 실행 중인가?
- [ ] HTTP Server가 포트 4000에서 실행 중인가?
- [ ] 방화벽이 포트를 차단하지 않는가?
- [ ] Python 클라이언트의 포트 설정이 맞는가?

### B. 데이터 전송 문제
- [ ] response.json 파일이 존재하는가?
- [ ] JSON 형식이 올바른가?
- [ ] HTTP 요청이 POST 메서드인가?
- [ ] 엔드포인트가 "/pose_data"인가?

### C. 데이터 처리 문제
- [ ] BP_Score 액터가 레벨에 배치되어 있는가?
- [ ] Custom Event가 올바르게 설정되어 있는가?
- [ ] Text Render Component가 연결되어 있는가?

## 8. 고급 설정

### A. 다중 포즈 데이터 처리
```blueprint
ReceivePoseData Event에서:
1. JSON Array에서 "points" 추출
2. For Each Loop로 각 포인트 처리
3. 포인트별 위치 정보를 3D 공간에 표시
```

### B. 실시간 시각화
```blueprint
1. 포즈 포인트를 3D 스피어로 표시
2. 포인트 간 연결선 그리기
3. Confidence 값에 따른 색상 변경
```

### C. 데이터 저장
```blueprint
1. File I/O 노드로 JSON 데이터 저장
2. 타임스탬프별 데이터 기록
3. 나중에 재생할 수 있는 형태로 저장
```

이 설정을 통해 언리얼 엔진에서 HTTP 서버를 구축하고 Python 클라이언트와 실시간으로 데이터를 주고받을 수 있습니다.
