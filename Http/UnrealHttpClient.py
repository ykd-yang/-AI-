import argparse
import json
import time
import requests
from pathlib import Path
from typing import Dict, Any


class UnrealHttpClient:
    """HTTP 클라이언트 - 5초마다 response.json 값을 언리얼 서버로 전송"""
    
    def __init__(self, unreal_host: str, unreal_port: int, json_file_path: str):
        self.unreal_host = unreal_host
        self.unreal_port = unreal_port
        self.json_file_path = Path(json_file_path)
        self.session = requests.Session()
        
        # HTTP 헤더 설정
        self.session.headers.update({
            'Content-Type': 'application/json; charset=utf-8',
            'Accept': 'application/json',
            'User-Agent': 'UnrealHttpClient/1.0'
        })
        
        # 언리얼 서버 URL 구성
        self.unreal_url = f"http://{unreal_host}:{unreal_port}/pose_data"
        
    def load_json_data(self) -> Dict[str, Any]:
        """response.json 파일에서 데이터 로드"""
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
        print(f"[UnrealClient] 시작 - {self.unreal_url}로 {interval}초마다 전송")
        print(f"[UnrealClient] JSON 파일: {self.json_file_path}")
        print(f"[UnrealClient] 중지하려면 Ctrl+C를 누르세요")
        
        try:
            while True:
                # JSON 데이터 로드
                try:
                    data = self.load_json_data()
                    print(f"[UnrealClient] 데이터 로드 완료 - {len(data.get('points', []))}개 포인트")
                except Exception as e:
                    print(f"[UnrealClient] 데이터 로드 실패: {e}")
                    time.sleep(interval)
                    continue
                
                # 언리얼 서버로 전송
                success = self.send_to_unreal(data)
                if success:
                    print(f"[UnrealClient] 다음 전송까지 {interval}초 대기...")
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
    parser.add_argument("--unreal-port", type=int, default=8080, help="언리얼 서버 포트 (기본값: 8080)")
    parser.add_argument("--json-file", default="response.json", help="전송할 JSON 파일 경로 (기본값: response.json)")
    parser.add_argument("--interval", type=float, default=5.0, help="전송 간격 (초) (기본값: 5.0)")
    return parser.parse_args()


def main():
    args = parse_args()
    
    try:
        client = UnrealHttpClient(
            unreal_host=args.unreal_host,
            unreal_port=args.unreal_port,
            json_file_path=args.json_file
        )
        client.run(interval=args.interval)
    except Exception as e:
        print(f"[UnrealClient] 초기화 실패: {e}")


if __name__ == "__main__":
    main()
