#!/usr/bin/env python3
"""
언리얼 엔진 HTTP 서버 연결 테스트 스크립트
Python 클라이언트와 언리얼 서버 간의 연결을 테스트합니다.
"""

import requests
import json
import time
import sys
import random
from pathlib import Path


def test_connection(host="127.0.0.1", port=4000, timeout=5):
    """언리얼 서버 연결 테스트"""
    url = f"http://{host}:{port}/pose_data"
    
    print(f"[Test] 연결 테스트 시작: {url}")
    
    # 테스트 데이터 생성
    test_data = {
        "message": "연결 테스트 메시지",
        "test_mode": True,
        "timestamp": time.time(),
        "client_id": "test_client",
        "confidence": 0.95
    }
    
    try:
        response = requests.post(
            url,
            json=test_data,
            timeout=timeout,
            headers={'Content-Type': 'application/json'}
        )
        
        if response.status_code == 200:
            print(f"[Test] ✅ 연결 성공! (Status: {response.status_code})")
            print(f"[Test] 응답: {response.text}")
            return True
        else:
            print(f"[Test] ❌ 연결 실패 (Status: {response.status_code})")
            print(f"[Test] 응답: {response.text}")
            return False
            
    except requests.exceptions.ConnectionError:
        print(f"[Test] ❌ 연결 실패: {host}:{port}에 연결할 수 없습니다")
        print(f"[Test] 언리얼 엔진이 실행 중이고 HTTP 서버가 시작되었는지 확인하세요")
        return False
    except requests.exceptions.Timeout:
        print(f"[Test] ❌ 타임아웃: 서버 응답 시간 초과 ({timeout}초)")
        return False
    except Exception as e:
        print(f"[Test] ❌ 예상치 못한 오류: {e}")
        return False


def test_with_response_json(host="127.0.0.1", port=4000):
    """response.json 파일을 사용한 테스트"""
    json_file = Path("response.json")
    
    if not json_file.exists():
        print(f"[Test] ❌ response.json 파일을 찾을 수 없습니다")
        print(f"[Test] 랜덤 데이터로 테스트를 진행합니다...")
        return test_with_random_data(host, port)
    
    print(f"[Test] response.json 파일을 사용한 테스트 시작")
    
    try:
        with open(json_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        # 테스트용 메타데이터 추가
        data['test_mode'] = True
        data['timestamp'] = time.time()
        data['client_id'] = 'test_client'
        
        url = f"http://{host}:{port}/pose_data"
        
        response = requests.post(
            url,
            json=data,
            timeout=10,
            headers={'Content-Type': 'application/json'}
        )
        
        if response.status_code == 200:
            points_count = len(data.get('last_status', {}).get('points', []))
            print(f"[Test] ✅ response.json 전송 성공!")
            print(f"[Test] 포인트 개수: {points_count}")
            print(f"[Test] 총 점수: {data.get('total_score', 'N/A')}")
            return True
        else:
            print(f"[Test] ❌ 전송 실패 (Status: {response.status_code})")
            return False
            
    except Exception as e:
        print(f"[Test] ❌ 오류: {e}")
        return False


def test_with_random_data(host="127.0.0.1", port=4000):
    """랜덤 데이터를 사용한 테스트"""
    print(f"[Test] 랜덤 데이터를 사용한 테스트 시작")
    
    try:
        # 랜덤 포즈 데이터 생성
        pose_names = ["pelvis", "spine_02", "neck_01", "head", "clavicle_l", "upperarm_l"]
        points = []
        
        for i, name in enumerate(pose_names):
            point = {
                "id": i,
                "name": name,
                "x": round(random.uniform(-2.0, 2.0), 3),
                "y": round(random.uniform(-2.0, 2.0), 3),
                "z": round(random.uniform(-2.0, 2.0), 3),
                "rotation": round(random.uniform(-180, 180), 1)
            }
            points.append(point)
        
        data = {
            "message": "랜덤 테스트 데이터",
            "total_score": round(random.uniform(0.0, 1.0), 2),
            "last_status": {
                "frame": random.randint(1, 100),
                "points": points
            },
            "part_score": [
                [round(random.uniform(0.0, 1.0), 1) for _ in range(4)]
            ],
            "timestamp": time.time(),
            "client_id": "test_random_client",
            "test_mode": True
        }
        
        url = f"http://{host}:{port}/pose_data"
        
        response = requests.post(
            url,
            json=data,
            timeout=10,
            headers={'Content-Type': 'application/json'}
        )
        
        if response.status_code == 200:
            print(f"[Test] ✅ 랜덤 데이터 전송 성공!")
            print(f"[Test] 포인트 개수: {len(points)}")
            print(f"[Test] 총 점수: {data['total_score']}")
            return True
        else:
            print(f"[Test] ❌ 전송 실패 (Status: {response.status_code})")
            return False
            
    except Exception as e:
        print(f"[Test] ❌ 오류: {e}")
        return False


def main():
    """메인 테스트 함수"""
    print("=" * 50)
    print("언리얼 엔진 HTTP 서버 연결 테스트")
    print("=" * 50)
    
    # 명령행 인수 처리
    host = "127.0.0.1"
    port = 4000
    
    if len(sys.argv) >= 2:
        host = sys.argv[1]
    if len(sys.argv) >= 3:
        port = int(sys.argv[2])
    
    print(f"[Test] 테스트 대상: {host}:{port}")
    print()
    
    # 1단계: 기본 연결 테스트
    print("1단계: 기본 연결 테스트")
    if not test_connection(host, port):
        print("\n[Test] ❌ 기본 연결 테스트 실패")
        print("[Test] 해결 방법:")
        print("  1. 언리얼 엔진이 실행 중인지 확인")
        print("  2. HTTP 서버가 포트 4000에서 실행 중인지 확인")
        print("  3. 방화벽 설정 확인")
        return
    
    print()
    
    # 2단계: response.json 파일 테스트
    print("2단계: response.json 파일 테스트")
    if test_with_response_json(host, port):
        print("\n[Test] ✅ 모든 테스트 통과!")
        print("[Test] 이제 UnrealHttpClient.py를 실행할 수 있습니다:")
        print(f"   python UnrealHttpClient.py --unreal-host {host} --unreal-port {port}")
    else:
        print("\n[Test] ❌ response.json 테스트 실패")
        print("[Test] response.json 파일 형식을 확인하세요")


if __name__ == "__main__":
    main()
