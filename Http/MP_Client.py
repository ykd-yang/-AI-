import argparse
import json
import socket

BUFFER_SIZE = 65536


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="MediaPipe pose tracking socket client")
    parser.add_argument("--host", default="127.0.0.1", help="Server host to connect to")
    parser.add_argument("--port", type=int, default=4000, help="Server port to connect to")
    parser.add_argument("--timeout", type=float, default=5.0, help="Socket timeout in seconds")
    return parser.parse_args()


def receive_stream(host: str, port: int, timeout: float) -> None:
    with socket.create_connection((host, port), timeout=timeout) as sock:
        print(f"[PoseClient] Connected to {host}:{port}")
        buffer = b""
        while True:
            chunk = sock.recv(BUFFER_SIZE)
            if not chunk:
                print("[PoseClient] Connection closed by server")
                break
            buffer += chunk
            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                if not line:
                    continue
                try:
                    message = json.loads(line.decode("utf-8"))
                except json.JSONDecodeError as exc:
                    print(f"[PoseClient] Failed to decode JSON: {exc}: {line!r}")
                    continue
                print(json.dumps(message, ensure_ascii=False))


def main() -> None:
    args = parse_args()
    try:
        receive_stream(args.host, args.port, args.timeout)
    except (ConnectionRefusedError, TimeoutError) as exc:
        print(f"[PoseClient] Could not connect to server: {exc}")
    except KeyboardInterrupt:
        print("\n[PoseClient] Interrupted by user")


if __name__ == "__main__":
    main()
