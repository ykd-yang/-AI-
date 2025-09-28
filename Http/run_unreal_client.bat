@echo off
echo UnrealHttpClient 실행 중...
echo.
echo 1. JSON 파일 모드 (response.json 사용)
echo 2. 랜덤 데이터 모드 (실시간 랜덤 포즈 데이터 생성)
echo.
set /p choice="모드를 선택하세요 (1 또는 2): "

if "%choice%"=="1" (
    echo JSON 파일 모드로 실행합니다...
    echo 언리얼 서버로 5초마다 response.json 데이터를 전송합니다.
    python UnrealHttpClient.py
) else if "%choice%"=="2" (
    echo 랜덤 데이터 모드로 실행합니다...
    echo 언리얼 서버로 5초마다 랜덤 포즈 데이터를 전송합니다.
    python UnrealHttpClient.py --random
) else (
    echo 잘못된 선택입니다. 기본 모드(JSON 파일)로 실행합니다.
    python UnrealHttpClient.py
)

echo.
echo 중지하려면 Ctrl+C를 누르세요.
pause
