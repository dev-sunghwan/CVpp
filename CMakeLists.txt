cmake_minimum_required(VERSION 3.10)
project(RTSP_Viewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(OpenCV_DIR "C:/Users/SungHwan/Desktop/CV++/opencv/build")
find_package(OpenCV REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
    app_config.cpp
    session_logger.cpp
)

set(GST_PREFIX "C:/Program Files/gstreamer/1.0/msvc_x86_64")
set(GST_INCLUDE_DIRS
    "${GST_PREFIX}/include/gstreamer-1.0"
    "${GST_PREFIX}/include/glib-2.0"
    "${GST_PREFIX}/lib/glib-2.0/include"
)
set(GST_LIBRARY_DIRS "${GST_PREFIX}/lib")
set(GST_LIBS gstreamer-1.0 gobject-2.0 glib-2.0 gstapp-1.0 gstrtsp-1.0)

target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS} ${GST_INCLUDE_DIRS})
target_link_directories(${PROJECT_NAME} PRIVATE ${GST_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS} ${GST_LIBS} ws2_32)
