cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC")

add_definitions(-Wall)

set (module vvdisplay)

if ( ANDROID )
include_directories(${ANDROID_ROOT}/vendor/nxp-opensource/libdrm-imx)
link_directories(${ANDROID_ROOT}/out/target/product/evk_8mp/vendor/lib64)
link_directories(${ANDROID_ROOT}/out/target/product/evk_8mp/system/lib64)
endif ()

if (WITH_DRM)
include(drm.cmake)
include_directories(
    ../../mediacontrol/buffer
    ../../mediacontrol/include
    ${DRM_HEADER_PATH}
)
add_definitions(-DWITH_DRM)
else(WITH_DRM)
include_directories(
    ../../mediacontrol/buffer
    ../../mediacontrol/include
)
endif()

if (NOT ANDROID)
    add_definitions(-DWAYLAND_SUPPORT)
    set (WAYLAND_SUPPORT 1)
else (NOT ANDROID)
    set (WAYLAND_SUPPORT 0)
endif (NOT ANDROID)

if (WAYLAND_SUPPORT)
    add_subdirectory(./wayland-client/)
    file(GLOB pub_headers *.h)
    file(GLOB libsources *.cpp ./wayland-client/*.c)
    include_directories(./wayland-client)
    target_link_libraries(wayland-client)
else (WAYLAND_SUPPORT)
    file(GLOB pub_headers *.h)
    file(GLOB libsources *.cpp)
endif (WAYLAND_SUPPORT)

add_library(${module}_shared SHARED ${libsources})

if( NOT DEFINED APPSHELL_TOP_COMPILE)
    add_subdirectory(${PROJECT_SOURCE_DIR}/../yuv yuv)
endif( NOT DEFINED APPSHELL_TOP_COMPILE)

if (WITH_DRM)
if (NOT ANDROID)
if (IMX_G2D)
add_custom_target(copy_monitor_cfg  ALL COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/display/VSI_Monitor.cfg ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/bin/VSI_Monitor.cfg)
target_link_libraries ( ${module}_shared yuv2rgb_static drm g2d ${LIB_PTHREAD_NAME})
else (IMX_G2D)
target_link_libraries ( ${module}_shared yuv2rgb_static drm ${LIB_PTHREAD_NAME})
endif (IMX_G2D)
else(NOT ANDROID)
target_link_libraries ( ${module}_shared yuv2rgb_static ${LIB_PTHREAD_NAME} drm_android cutils base c++)
endif(NOT ANDROID)
else(WITH_DRM)
target_link_libraries ( ${module}_shared yuv2rgb_static ${LIB_PTHREAD_NAME})
endif(WITH_DRM)

