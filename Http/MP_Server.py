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

mp_pose = mp.solutions.pose

MANNY17_NAMES: Tuple[str, ...] = (
    "pelvis",
    "spine_02",
    "neck_01",
    "head",
    "clavicle_l",
    "upperarm_l",
    "lowerarm_l",
    "hand_l",
    "clavicle_r",
    "upperarm_r",
    "lowerarm_r",
    "hand_r",
    "thigh_l",
    "calf_l",
    "foot_l",
    "thigh_r",
    "calf_r",
    "foot_r",
)

MANNY17_INDICES: Tuple[Tuple[int, int], ...] = (
    (mp_pose.PoseLandmark.LEFT_HIP.value, mp_pose.PoseLandmark.RIGHT_HIP.value),
    (mp_pose.PoseLandmark.LEFT_SHOULDER.value, mp_pose.PoseLandmark.RIGHT_SHOULDER.value),
    (mp_pose.PoseLandmark.NOSE.value, -1),
    (mp_pose.PoseLandmark.NOSE.value, -1),
    (mp_pose.PoseLandmark.LEFT_SHOULDER.value, -1),
    (mp_pose.PoseLandmark.LEFT_ELBOW.value, -1),
    (mp_pose.PoseLandmark.LEFT_WRIST.value, -1),
    (mp_pose.PoseLandmark.LEFT_INDEX.value, -1),
    (mp_pose.PoseLandmark.RIGHT_SHOULDER.value, -1),
    (mp_pose.PoseLandmark.RIGHT_ELBOW.value, -1),
    (mp_pose.PoseLandmark.RIGHT_WRIST.value, -1),
    (mp_pose.PoseLandmark.RIGHT_INDEX.value, -1),
    (mp_pose.PoseLandmark.LEFT_HIP.value, -1),
    (mp_pose.PoseLandmark.LEFT_KNEE.value, -1),
    (mp_pose.PoseLandmark.LEFT_ANKLE.value, -1),
    (mp_pose.PoseLandmark.RIGHT_HIP.value, -1),
    (mp_pose.PoseLandmark.RIGHT_KNEE.value, -1),
    (mp_pose.PoseLandmark.RIGHT_ANKLE.value, -1),
)

MANNY17_CONNECTIONS: Tuple[Tuple[int, int], ...] = (
    (0, 1),
    (1, 2),
    (2, 3),
    (2, 4),
    (4, 5),
    (5, 6),
    (6, 7),
    (2, 8),
    (8, 9),
    (9, 10),
    (10, 11),
    (0, 12),
    (12, 13),
    (13, 14),
    (0, 15),
    (15, 16),
    (16, 17),
)

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
        self._accept_thread = threading.Thread(target=self._accept_loop, name="PoseServerAccept", daemon=True)
        self._accept_thread.start()
        print(f"[PoseServer] Listening on {self._host}:{self._port}")

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
            print(f"[PoseServer] Client connected: {addr}")
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
                print(f"[PoseServer] Client disconnected: {peer}")
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


def _landmark_to_point(lm: landmark_pb2.NormalizedLandmark) -> Tuple[float, float, float, float]:
    return (lm.x, lm.y, lm.z, getattr(lm, "visibility", 0.0))


def _average_pair(a: Tuple[float, float, float, float], b: Tuple[float, float, float, float]) -> Tuple[float, float, float, float]:
    return (
        (a[0] + b[0]) * 0.5,
        (a[1] + b[1]) * 0.5,
        (a[2] + b[2]) * 0.5,
        (a[3] + b[3]) * 0.5,
    )


def compute_manny17_joints(landmarks: Sequence[landmark_pb2.NormalizedLandmark]) -> List[Dict[str, object]]:
    if not landmarks:
        return []

    joints: List[Dict[str, object]] = []
    for joint_id, (name, (a_idx, b_idx)) in enumerate(zip(MANNY17_NAMES, MANNY17_INDICES)):
        base_point = _landmark_to_point(landmarks[a_idx])
        if b_idx >= 0:
            pair_point = _landmark_to_point(landmarks[b_idx])
            joint_point = _average_pair(base_point, pair_point)
        else:
            joint_point = base_point
        joints.append(
            {
                "id": joint_id,
                "name": name,
                "x": joint_point[0],
                "y": joint_point[1],
                "z": joint_point[2],
                "visibility": joint_point[3],
            }
        )
    return joints


def draw_manny17_visualization(frame: np.ndarray, joints: Sequence[Dict[str, object]]) -> np.ndarray:
    """Render Manny 17-joint skeleton with red joints and green bones."""
    image = frame.copy()
    h, w = image.shape[:2]
    points: Dict[int, Tuple[int, int]] = {}
    for joint in joints:
        joint_id = int(joint["id"])
        px = int(float(joint["x"]) * w)
        py = int(float(joint["y"]) * h)
        points[joint_id] = (px, py)
        cv2.circle(image, (px, py), 6, (0, 0, 255), thickness=-1)

    for start_id, end_id in MANNY17_CONNECTIONS:
        if start_id in points and end_id in points:
            cv2.line(image, points[start_id], points[end_id], (0, 255, 0), thickness=2)
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


def run_pose_server(host: str, port: int, visualize: bool, camera_index: int, video_path: str | None, loop_video: bool) -> None:
    broadcaster = LandmarkBroadcaster(host, port)
    broadcaster.start()

    pose = mp_pose.Pose(
        model_complexity=1,
        smooth_landmarks=True,
        enable_segmentation=False,
        min_detection_confidence=0.5,
        min_tracking_confidence=0.5,
    )

    cap, source_desc = _open_capture(camera_index, video_path)
    if not cap.isOpened():
        pose.close()
        raise RuntimeError(f"Could not open {source_desc}")

    print(f"[PoseServer] Reading frames from {source_desc}")

    try:
        while True:
            success, frame = cap.read()
            if not success:
                if video_path:
                    if loop_video:
                        cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                        continue
                    print("[PoseServer] Reached end of video; stopping.")
                    break
                print("[PoseServer] Frame capture failed; retrying...")
                time.sleep(0.05)
                continue

            rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            results = pose.process(rgb_frame)

            vis_frame = None
            if results.pose_landmarks:
                joints = compute_manny17_joints(results.pose_landmarks.landmark)
                if joints:
                    payload = {
                        "timestamp": time.time(),
                        "points": joints,
                    }
                    broadcaster.broadcast(json.dumps(payload))
                    if visualize:
                        vis_frame = draw_manny17_visualization(frame, joints)

            if visualize:
                if vis_frame is None:
                    vis_frame = np.zeros_like(frame)
                cv2.imshow("MediaPipe Pose", vis_frame)
                if cv2.waitKey(1) & 0xFF == 27:
                    break
    except KeyboardInterrupt:
        print("\n[PoseServer] Interrupted by user.")
    finally:
        broadcaster.stop()
        pose.close()
        cap.release()
        if visualize:
            cv2.destroyAllWindows()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="MediaPipe pose tracking socket server (Manny 17 layout)")
    parser.add_argument("--host", default="127.0.0.1", help="Host/IP to bind the TCP server")
    parser.add_argument("--port", type=int, default=4000, help="Port for the TCP server")
    parser.add_argument("--camera", type=int, default=0, help="Camera index to open when no video is provided")
    parser.add_argument("--video", "--source", dest="video", help="Path to a video/GIF file or directory (overrides camera)")
    parser.add_argument("--loop-video", action="store_true", help="Loop the provided video file when it ends")
    parser.add_argument("--visualize", action="store_true", help="Enable OpenCV window for Manny 17 visualization")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    try:
        run_pose_server(args.host, args.port, args.visualize, args.camera, args.video, args.loop_video)
    except FileNotFoundError as exc:
        print(f"[PoseServer] {exc}")


if __name__ == "__main__":
    main()
