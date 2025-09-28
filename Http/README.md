# HTTP 통신 프로젝트

이 프로젝트는 MediaPipe 포즈 추적과 HTTP 통신을 통해 언리얼 엔진과 데이터를 주고받는 시스템입니다.

## 파일 구성

- `MP_Server.py`: MediaPipe 포즈 추적 TCP 서버
- `MP_Client.py`: MediaPipe 포즈 데이터 수신 클라이언트
- `HttpServer.py`: Flask 기반 HTTP 서버
- `UnrealHttpClient.py`: **새로 추가** - 5초마다 response.json을 언리얼 서버로 전송하는 클라이언트
- `response.json`: 포즈 데이터 JSON 파일

## UnrealHttpClient 사용법

### 기본 사용법
```bash
python UnrealHttpClient.py
```

### 옵션 설정
```bash
# 언리얼 서버 주소와 포트 변경
python UnrealHttpClient.py --unreal-host 192.168.1.100 --unreal-port 9000

# 전송 간격 변경 (10초마다)
python UnrealHttpClient.py --interval 10.0

# 다른 JSON 파일 사용
python UnrealHttpClient.py --json-file my_pose_data.json

# 랜덤 데이터 생성 모드 (JSON 파일 대신 실시간 랜덤 포즈 데이터 생성)
python UnrealHttpClient.py --random

# 랜덤 모드 + 빠른 전송 (1초마다)
python UnrealHttpClient.py --random --interval 1.0
```

### 전체 옵션
- `--unreal-host`: 언리얼 서버 호스트 (기본값: 127.0.0.1)
- `--unreal-port`: 언리얼 서버 포트 (기본값: 4000)
- `--json-file`: 전송할 JSON 파일 경로 (기본값: response.json)
- `--interval`: 전송 간격 (초) (기본값: 5.0)
- `--random`: 랜덤 데이터 생성 모드 (JSON 파일 대신 실시간 랜덤 포즈 데이터 생성)

## 기능

### UnrealHttpClient 주요 기능
1. **자동 JSON 로드**: response.json 파일에서 포즈 데이터를 자동으로 읽어옴
2. **랜덤 데이터 생성**: `--random` 옵션으로 실시간 랜덤 포즈 데이터 생성
3. **주기적 전송**: 설정된 간격(기본 5초)마다 언리얼 서버로 HTTP POST 요청
4. **타임스탬프 추가**: 각 전송에 현재 시간과 클라이언트 ID 추가
5. **에러 처리**: 연결 실패, 타임아웃, JSON 파싱 오류 등에 대한 적절한 처리
6. **실시간 상태 출력**: 전송 성공/실패 상태를 콘솔에 실시간 출력

### 전송 데이터 형식
```json
{
  "message": "랜덤 데이터 전송_123",
  "total_score": 0.75,
  "last_status": {
    "frame": 42,
    "points": [
      {"id": 0, "name": "pelvis", "x": 0.123, "y": -0.456, "z": 0.789, "rotation": 45.2},
      {"id": 1, "name": "spine_02", "x": 0.234, "y": -0.567, "z": 0.890, "rotation": -30.1}
    ]
  },
  "part_score": [
    [0.2, 0.6, 0.0, 0.8],
    [0.1, 0.5, 0.1, 0.5]
  ],
  "timestamp": 1640995200.123,
  "client_id": "python_random_client"
}
```

## 설치 및 실행

### 1. 의존성 설치
```bash
pip install -r requirements.txt
```

### 2. 언리얼 서버 준비
언리얼 엔진에서 HTTP 서버를 `/pose_data` 엔드포인트로 설정하세요.

### 3. 클라이언트 실행
```bash
python UnrealHttpClient.py
```

## 예제 시나리오

1. **개발 환경**: 
   ```bash
   python UnrealHttpClient.py --unreal-host 127.0.0.1 --unreal-port 4000
   ```

2. **프로덕션 환경**:
   ```bash
   python UnrealHttpClient.py --unreal-host 127.0.0.1 --unreal-port 4000 --interval 3.0
   ```

3. **랜덤 데이터 테스트**:
   ```bash
   python UnrealHttpClient.py --random --interval 2.0
   ```

## 문제 해결

- **연결 실패**: 언리얼 서버가 실행 중인지 확인하고 포트 번호가 맞는지 확인
- **JSON 파싱 오류**: response.json 파일이 올바른 JSON 형식인지 확인
- **전송 실패**: 네트워크 연결 상태와 언리얼 서버의 엔드포인트 설정 확인
