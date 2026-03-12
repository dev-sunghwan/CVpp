import cv2
import os

print(f"OpenCV version: {cv2.__version__}")

# FFMPEG expects 'headers' to be separated by \r\n
# OpenCV passes this environment variable to libavformat (ffmpeg)
# The format for OPENCV_FFMPEG_CAPTURE_OPTIONS is: "key;value|key;value"
# So key="headers" and value="Rate-Control: no\r\n"
opts = "headers;Rate-Control: no\r\n"
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
