import cv2
import os

print(f"OpenCV version: {cv2.__version__}")

# FFmpeg 꼼수: User-Agent 옵션 끝에 \r\n 을 붙이고 다른 헤더를 몰래 주입하는 해킹 방식!
# 이것이 먹히면 C++ 멀티스레드 프록시 서버를 만들지 않아도 됩니다.
hack_ua = "Lavf58.76.100\r\nRate-Control: no\r\nScale: 2.0"
opts = f"rtsp_transport;tcp|user_agent;{hack_ua}"
os.environ["OPENCV_FFMPEG_CAPTURE_OPTIONS"] = opts

source = "rtsp://admin:Sunap1!!@192.168.4.225/profile10/media.smp"

cap = cv2.VideoCapture(source, cv2.CAP_FFMPEG)
if not cap.isOpened():
    print("Failed to open.")
else:
    print("Successfully opened. Check Wireshark now.")
    for i in range(100):
        ret, frame = cap.read()
        if ret:
            print(f"Got frame {i+1}/100.", end="\r")
        else:
            print(f"\nFailed on frame {i+1}")
            break
    print("\nCapture finished.")
cap.release()
