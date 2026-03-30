cmake_minimum_required(VERSION 3.10)
project(RTSP_Viewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(OpenCV_DIR "C:/Users/SungHwan/Desktop/CV++/opencv/build")
find_package(OpenCV REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
    app_config.cpp
    session_logger.cpp
    metadata_parser.cpp
    overlay_state.cpp
    video_rtsp_session.cpp
    metadata_rtsp_session.cpp
)

add_executable(metadata_probe
    metadata_probe.cpp
    app_config.cpp
    metadata_parser.cpp
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

target_include_directories(metadata_probe PRIVATE ${GST_INCLUDE_DIRS})
target_link_directories(metadata_probe PRIVATE ${GST_LIBRARY_DIRS})
target_link_libraries(metadata_probe PRIVATE ${GST_LIBS} ws2_32)

set(QT_ROOT_HINT "C:/Qt/6.8.3/msvc2022_64")
if (EXISTS "${QT_ROOT_HINT}/lib/cmake/Qt6")
    set(Qt6_DIR "${QT_ROOT_HINT}/lib/cmake/Qt6")
endif()

find_package(Qt6 COMPONENTS Widgets QUIET)

if (Qt6_FOUND)
    add_executable(CVPP_QtShell
        qt_shell_main.cpp
        qt_shell_window.cpp
        qt_shell_window.h
        app_config.cpp
        session_logger.cpp
        metadata_parser.cpp
        overlay_state.cpp
        video_rtsp_session.cpp
        metadata_rtsp_session.cpp
    )

    target_include_directories(CVPP_QtShell PRIVATE ${OpenCV_INCLUDE_DIRS} ${GST_INCLUDE_DIRS})
    target_link_directories(CVPP_QtShell PRIVATE ${GST_LIBRARY_DIRS})
    target_link_libraries(CVPP_QtShell PRIVATE Qt6::Widgets ${OpenCV_LIBS} ${GST_LIBS} ws2_32)
else()
    message(STATUS "Qt6 Widgets not found; skipping CVPP_QtShell target")
endif()
