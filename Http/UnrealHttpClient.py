import argparse
import json
import time
import requests
import random
from pathlib import Path
from typing import Dict, Any


class UnrealHttpClient:
    """HTTP 클라이언트 - 5초마다 response.json 값을 언리얼 서버로 전송"""
    
    def __init__(self, unreal_host: str, unreal_port: int, json_file_path: str, use_random: bool = False):
        self.unreal_host = unreal_host
        self.unreal_port = unreal_port
        self.json_file_path = Path(json_file_path)
        self.use_random = use_random
        self.session = requests.Session()
        
        # HTTP 헤더 설정
        self.session.headers.update({
            'Content-Type': 'application/json; charset=utf-8',
            'Accept': 'application/json',
            'User-Agent': 'UnrealHttpClient/1.0'
        })
        
        # 언리얼 서버 URL 구성
        self.unreal_url = f"http://{unreal_host}:{unreal_port}/pose_data"
        
        # 포즈 포인트 이름들 (MANNY17 구조 기반)
        self.pose_names = [
            "pelvis", "spine_02", "neck_01", "head", "clavicle_l", "upperarm_l", 
            "lowerarm_l", "hand_l", "clavicle_r", "upperarm_r", "lowerarm_r", 
            "hand_r", "thigh_l", "calf_l", "foot_l", "thigh_r", "calf_r", "foot_r"
        ]
        
    def generate_random_pose_data(self) -> Dict[str, Any]:
        """랜덤 포즈 데이터 생성"""
        # 랜덤 포인트 생성
        points = []
        for i, name in enumerate(self.pose_names):
            point = {
                "id": i,
                "name": name,
                "x": round(random.uniform(-2.0, 2.0), 3),
                "y": round(random.uniform(-2.0, 2.0), 3),
                "z": round(random.uniform(-2.0, 2.0), 3),
                "rotation": round(random.uniform(-180, 180), 1)
            }
            points.append(point)
        
        # 랜덤 part_score 생성 (4개 값)
        part_scores = []
        for _ in range(random.randint(1, 6)):
            part_score = [
                round(random.uniform(0.0, 1.0), 1),
                round(random.uniform(0.0, 1.0), 1),
                round(random.uniform(0.0, 1.0), 1),
                round(random.uniform(0.0, 1.0), 1)
            ]
            part_scores.append(part_score)
        
        data = {
            "message": f"랜덤 데이터 전송_{random.randint(1, 1000)}",
            "total_score": round(random.uniform(0.0, 1.0), 2),
            "last_status": {
                "frame": random.randint(1, 100),
                "points": points
            },
            "part_score": part_scores,
            "timestamp": time.time(),
            "client_id": "python_random_client"
        }
        
        return data
    
    def load_json_data(self) -> Dict[str, Any]:
        """response.json 파일에서 데이터 로드 또는 랜덤 데이터 생성"""
        if self.use_random:
            return self.generate_random_pose_data()
            
        try:
            if not self.json_file_path.exists():
                raise FileNotFoundError(f"JSON 파일을 찾을 수 없습니다: {self.json_file_path}")
                
            with open(self.json_file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
                
            # 타임스탬프 추가
            data['timestamp'] = time.time()
            data['client_id'] = 'python_client'
            
            return data
            
        except json.JSONDecodeError as e:
            raise ValueError(f"JSON 파일 파싱 오류: {e}")
        except Exception as e:
            raise RuntimeError(f"JSON 파일 로드 오류: {e}")
    
    def send_to_unreal(self, data: Dict[str, Any]) -> bool:
        """언리얼 서버로 데이터 전송"""
        try:
            response = self.session.post(
                self.unreal_url,
                json=data,
                timeout=10
            )
            
            if response.status_code == 200:
                print(f"[UnrealClient] 성공적으로 전송됨 (Status: {response.status_code})")
                return True
            else:
                print(f"[UnrealClient] 전송 실패 (Status: {response.status_code}): {response.text}")
                return False
                
        except requests.exceptions.ConnectionError:
            print(f"[UnrealClient] 연결 실패: {self.unreal_url}에 연결할 수 없습니다")
            return False
        except requests.exceptions.Timeout:
            print(f"[UnrealClient] 타임아웃: 서버 응답 시간 초과")
            return False
        except Exception as e:
            print(f"[UnrealClient] 전송 오류: {e}")
            return False
    
    def run(self, interval: float = 5.0):
        """주기적으로 데이터 전송 실행"""
        mode_text = "랜덤 데이터 생성 모드" if self.use_random else f"JSON 파일 모드 ({self.json_file_path})"
        print(f"[UnrealClient] 시작 - {self.unreal_url}로 {interval}초마다 전송")
        print(f"[UnrealClient] 모드: {mode_text}")
        print(f"[UnrealClient] 중지하려면 Ctrl+C를 누르세요")
        
        try:
            while True:
                # JSON 데이터 로드 또는 생성
                try:
                    data = self.load_json_data()
                    points_count = len(data.get('last_status', {}).get('points', []))
                    message = data.get('message', 'N/A')
                    print(f"[UnrealClient] 데이터 준비 완료 - {points_count}개 포인트, 메시지: {message}")
                except Exception as e:
                    print(f"[UnrealClient] 데이터 로드 실패: {e}")
                    time.sleep(interval)
                    continue
                
                # 언리얼 서버로 전송
                success = self.send_to_unreal(data)
                if success:
                    print(f"[UnrealClient] 전송 성공! 다음 전송까지 {interval}초 대기...")
                else:
                    print(f"[UnrealClient] 전송 실패, {interval}초 후 재시도...")
                
                time.sleep(interval)
                
        except KeyboardInterrupt:
            print("\n[UnrealClient] 사용자에 의해 중지됨")
        except Exception as e:
            print(f"[UnrealClient] 예상치 못한 오류: {e}")
        finally:
            self.session.close()
            print("[UnrealClient] 클라이언트 종료")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="언리얼 서버로 response.json 데이터를 주기적으로 전송하는 HTTP 클라이언트")
    parser.add_argument("--unreal-host", default="127.0.0.1", help="언리얼 서버 호스트 (기본값: 127.0.0.1)")
    parser.add_argument("--unreal-port", type=int, default=4000, help="언리얼 서버 포트 (기본값: 4000)")
    parser.add_argument("--json-file", default="response.json", help="전송할 JSON 파일 경로 (기본값: response.json)")
    parser.add_argument("--interval", type=float, default=5.0, help="전송 간격 (초) (기본값: 5.0)")
    parser.add_argument("--random", action="store_true", help="랜덤 데이터 생성 모드 (JSON 파일 대신 랜덤 포즈 데이터 생성)")
    return parser.parse_args()


def main():
    args = parse_args()
    
    try:
        client = UnrealHttpClient(
            unreal_host=args.unreal_host,
            unreal_port=args.unreal_port,
            json_file_path=args.json_file,
            use_random=args.random
        )
        client.run(interval=args.interval)
    except Exception as e:
        print(f"[UnrealClient] 초기화 실패: {e}")


if __name__ == "__main__":
    main()
