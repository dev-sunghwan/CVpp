cmake_minimum_required(VERSION 3.10)
project(RTSP_Viewer)

# C++17 권장
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# OpenCV가 설치된 경로 지정 (백그라운드에서 다운로드 중인 경로)
set(OpenCV_DIR "C:/Users/SungHwan/Desktop/CV++/opencv/build")

# OpenCV 패키지 찾기
find_package(OpenCV REQUIRED)

# 메인 실행 파일 생성
add_executable(${PROJECT_NAME} main.cpp)

# GStreamer 1.28 MSVC 경로 설정 (설치된 기본 폴더)
set(GST_PREFIX "C:/Program Files/gstreamer/1.0/msvc_x86_64")
set(GST_INCLUDE_DIRS 
    "${GST_PREFIX}/include/gstreamer-1.0"
    "${GST_PREFIX}/include/glib-2.0"
    "${GST_PREFIX}/lib/glib-2.0/include"
)
set(GST_LIBRARY_DIRS "${GST_PREFIX}/lib")

# 링크할 GStreamer 기본 라이브러리들
set(GST_LIBS gstreamer-1.0 gobject-2.0 glib-2.0 gstapp-1.0 gstrtsp-1.0)

# OpenCV 및 GStreamer 헤더 추가
target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS} ${GST_INCLUDE_DIRS})

# GStreamer 라이브러리 폴더 및 링크 추가
target_link_directories(${PROJECT_NAME} PRIVATE ${GST_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS} ${GST_LIBS} ws2_32)
