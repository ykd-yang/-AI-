import argparse
import json
import socket
import threading
import time
from contextlib import closing
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

import cv2
import mediapipe as mp
from mediapipe.framework.formats import landmark_pb2
import numpy as np

mp_hands = mp.solutions.hands
mp_drawing = mp.solutions.drawing_utils
mp_drawing_styles = mp.solutions.drawing_styles

VIDEO_EXTENSIONS: Tuple[str, ...] = (".mp4", ".mov", ".avi", ".mkv", ".gif")


class LandmarkBroadcaster:
    """TCP server that broadcasts JSON lines to all connected clients."""

    def __init__(self, host: str, port: int) -> None:
        self._host = host
        self._port = port
        self._server_socket: socket.socket | None = None
        self._clients: List[socket.socket] = []
        self._clients_lock = threading.Lock()
        self._accept_thread: threading.Thread | None = None
        self._stop_event = threading.Event()

    def start(self) -> None:
        if self._server_socket is not None:
            return
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.bind((self._host, self._port))
        server_socket.listen()
        self._server_socket = server_socket
        self._accept_thread = threading.Thread(target=self._accept_loop, name="HandServerAccept", daemon=True)
        self._accept_thread.start()
        print(f"[HandServer] Listening on {self._host}:{self._port}")

    def _accept_loop(self) -> None:
        assert self._server_socket is not None
        while not self._stop_event.is_set():
            try:
                self._server_socket.settimeout(0.5)
                conn, addr = self._server_socket.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            print(f"[HandServer] Client connected: {addr}")
            conn.setblocking(True)
            with self._clients_lock:
                self._clients.append(conn)

    def broadcast(self, payload: str) -> None:
        if not payload.endswith("\n"):
            payload += "\n"
        dead_clients: List[socket.socket] = []
        with self._clients_lock:
            for client in self._clients:
                try:
                    client.sendall(payload.encode("utf-8"))
                except OSError:
                    dead_clients.append(client)
            for dead in dead_clients:
                try:
                    peer = dead.getpeername()
                except OSError:
                    peer = "unknown"
                print(f"[HandServer] Client disconnected: {peer}")
                self._clients.remove(dead)
                with closing(dead):
                    pass

    def stop(self) -> None:
        self._stop_event.set()
        if self._server_socket:
            try:
                self._server_socket.close()
            except OSError:
                pass
        if self._accept_thread:
            self._accept_thread.join(timeout=1.0)
        with self._clients_lock:
            for client in self._clients:
                try:
                    client.close()
                except OSError:
                    pass
            self._clients.clear()


def compute_hand_payload(multi_hand_landmarks, multi_handedness) -> Dict[str, object]:
    hands_payload = []
    for i, (landmarks, handedness) in enumerate(zip(multi_hand_landmarks, multi_handedness)):
        hand_payload = {
            "hand_index": i,
            "thumb_x": [landmarks.landmark[i].x for i in range(mp_hands.HandLandmark.THUMB_CMC, mp_hands.HandLandmark.THUMB_TIP + 1)],
            "index_x": [landmarks.landmark[i].x for i in range(mp_hands.HandLandmark.INDEX_FINGER_MCP, mp_hands.HandLandmark.INDEX_FINGER_TIP + 1)],
        }
        hands_payload.append(hand_payload)

    return {
        "timestamp": time.time(),
        "hands": hands_payload,
    }


def draw_hand_visualization(frame: np.ndarray, multi_hand_landmarks) -> np.ndarray:
    image = frame.copy()
    if multi_hand_landmarks:
        for hand_landmarks in multi_hand_landmarks:
            mp_drawing.draw_landmarks(
                image,
                hand_landmarks,
                mp_hands.HAND_CONNECTIONS,
                mp_drawing_styles.get_default_hand_landmarks_style(),
                mp_drawing_styles.get_default_hand_connections_style())
    return image


def _resolve_video_path(video_path: str) -> Tuple[str, str]:
    path = Path(video_path).expanduser()
    if path.is_dir():
        for candidate in sorted(path.iterdir()):
            if candidate.is_file() and candidate.suffix.lower() in VIDEO_EXTENSIONS:
                resolved = candidate.resolve()
                return str(resolved), f"video file '{resolved}'"
        raise FileNotFoundError(f"No supported video files found in {path}")
    if not path.is_file():
        raise FileNotFoundError(f"Video path does not exist: {path}")
    resolved = path.resolve()
    return str(resolved), f"video file '{resolved}'"


def _open_capture(camera_index: int, video_path: str | None) -> Tuple[cv2.VideoCapture, str]:
    if video_path:
        resolved_path, source_desc = _resolve_video_path(video_path)
        capture = cv2.VideoCapture(resolved_path)
    else:
        capture = cv2.VideoCapture(camera_index)
        source_desc = f"camera index {camera_index}"
    return capture, source_desc


def run_hand_server(host: str, port: int, visualize: bool, camera_index: int, video_path: str | None, loop_video: bool) -> None:
    broadcaster = LandmarkBroadcaster(host, port)
    broadcaster.start()

    with mp_hands.Hands(
        model_complexity=0,
        min_detection_confidence=0.5,
        min_tracking_confidence=0.5) as hands:

        cap, source_desc = _open_capture(camera_index, video_path)
        if not cap.isOpened():
            raise RuntimeError(f"Could not open {source_desc}")

        print(f"[HandServer] Reading frames from {source_desc}")

        try:
            while True:
                success, frame = cap.read()
                if not success:
                    if video_path:
                        if loop_video:
                            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                            continue
                        print("[HandServer] Reached end of video; stopping.")
                        break
                    print("[HandServer] Frame capture failed; retrying...")
                    time.sleep(0.05)
                    continue

                # To improve performance, optionally mark the image as not writeable to
                # pass by reference.
                frame.flags.writeable = False
                rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                results = hands.process(rgb_frame)
                frame.flags.writeable = True

                vis_frame = None
                if results.multi_hand_landmarks:
                    payload = compute_hand_payload(results.multi_hand_landmarks, results.multi_handedness)
                    broadcaster.broadcast(json.dumps(payload))
                    if visualize:
                        vis_frame = draw_hand_visualization(frame, results.multi_hand_landmarks)

                if visualize:
                    if vis_frame is None:
                        vis_frame = frame.copy() # Show original frame if no hands
                    cv2.imshow("MediaPipe Hands", vis_frame)
                    if cv2.waitKey(1) & 0xFF == 27:
                        break
        except KeyboardInterrupt:
            print("\n[HandServer] Interrupted by user.")
        finally:
            broadcaster.stop()
            cap.release()
            if visualize:
                cv2.destroyAllWindows()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="MediaPipe hand tracking socket server")
    parser.add_argument("--host", default="127.0.0.1", help="Host/IP to bind the TCP server")
    parser.add_argument("--port", type=int, default=4000, help="Port for the TCP server")
    parser.add_argument("--camera", type=int, default=0, help="Camera index to open when no video is provided")
    parser.add_argument("--video", "--source", dest="video", help="Path to a video/GIF file or directory (overrides camera)")
    parser.add_argument("--loop-video", action="store_true", help="Loop the provided video file when it ends")
    parser.add_argument("--visualize", action="store_true", help="Enable OpenCV window for hand visualization")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    try:
        run_hand_server(args.host, args.port, args.visualize, args.camera, args.video, args.loop_video)
    except FileNotFoundError as exc:
        print(f"[HandServer] {exc}")


if __name__ == "__main__":
    main()