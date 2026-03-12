import urllib.request
import json
import subprocess
import os
import sys

# Test if FFmpeg CLI can inject custom RTSP headers
# Usually FFmpeg supports -headers option for HTTP, but for RTSP it's sometimes tricky.
# Some versions of FFmpeg support -rtsp_flags custom_headers and -headers
# Let's write a batch script to test this with ffprobe first.

# Create a test script
test_script = """
@echo off
set FFMPEG_PATH=C:\\Users\\SungHwan\\Desktop\\CV++\\build\\Release\\opencv_videoio_ffmpeg4100_64.dll
echo Testing FFmpeg options...
"""
with open("test_ffmpeg.bat", "w") as f:
    f.write(test_script)
print("Created test script.")
