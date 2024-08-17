cmake_minimum_required(VERSION 2.6)

# define application name
set (application  tuningext)

if(DEFINED APPMODE)
    if(${APPMODE} STREQUAL V4L2)
    message("========== APPMODE set to V4L2 ==========")
        set(APPMODE_V4L2 1)
        add_definitions(-DAPPMODE_V4L2)
    elseif(${APPMODE} STREQUAL NATIVE)
        message("==========APPMODE set to  NATIVE==========")
        set(APPMODE_NATIVE 1)
        add_definitions(-DAPPMODE_NATIVE)
    else(${APPMODE} STREQUAL V4L2)
        message(FATAL_ERROR "CFG_ERROR APPMODE=${APPMODE} error, can not found configuration settings, exit")
    endif (${APPMODE} STREQUAL V4L2)
else(DEFINED APPMODE)
    message("==========APPMODE DEFAILT to  NATIVE==========")
    set(APPMODE_NATIVE 1)
    add_definitions(-DAPPMODE_NATIVE)
endif(DEFINED APPMODE)

if(CMAKE_BUILD_TYPE STREQUAL RELEASE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DQT_NO_DEBUG_OUTPUT -DQT_NO_WARNING_OUTPUT")
else(CMAKE_BUILD_TYPE STREQUAL RELEASE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC")
endif(CMAKE_BUILD_TYPE STREQUAL RELEASE)

option(CTRL_DEWARP "Dewarp Control" OFF)

set(CMAKE_CXX_FLAGS_DEBUG "-fno-strict-aliasing -O3")
set(CMAKE_C_FLAGS_DEBUG "-fno-strict-aliasing -O3")

if(DEFINED PARTITION_BUILD)
    if(${PARTITION_BUILD} STREQUAL 0)
        set(GENERATE_PARTITION_BUILD 0)
    else(${PARTITION_BUILD} STREQUAL 0)
        set(GENERATE_PARTITION_BUILD 1)
    endif(${PARTITION_BUILD} STREQUAL 0)
else(DEFINED PARTITION_BUILD)
    #default value of GENERATE_PARTITION_BUILD
    set(GENERATE_PARTITION_BUILD 1)
endif(DEFINED PARTITION_BUILD)

if(${GENERATE_PARTITION_BUILD} STREQUAL 0)
    #link libs headers
    set(UNITS_HEADER_FILES ${CMAKE_HOME_DIRECTORY}/shell_libs/include/units_headers/)
    set(SHELL_LIBS_DIRECTORY ${CMAKE_HOME_DIRECTORY}/shell_libs/${APPMODE}/ispcore/${PLATFORM})

    #set(SHELL_LIBS_LIST
    #a2dnr a3dnr adpcc adpf aec aee aflt af ahdr avs awb awdr3 base64 bufferpool bufsync_ctrl cam_calibdb cam_device
    #cam_engine cameric_drv cameric_reg_drv cim_ctrl common ebase fpga hal i2c_drv ibd isi mim_ctrl
    #mipi_drv mom_ctrl oslayer tinyxml2
    #)
    set(SHELL_LIBS_LIST tinyxml2 ebase oslayer)

else(${GENERATE_PARTITION_BUILD} STREQUAL 0)
    #full src compile
    set(UNITS_HEADER_FILES )
    set(SHELL_LIBS_DIRECTORY )
    set(SHELL_LIBS_LIST )
endif(${GENERATE_PARTITION_BUILD} STREQUAL 0)

if ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
    set(server_libs
        jsoncpp_lib tinyxml2 ebase oslayer_shared
    )

else ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
    set(server_libs
        jsoncpp_lib tinyxml2_shared ebase_shared oslayer_shared
    )

endif ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))

set(CONFIG_PATH config)

add_definitions(-DHAL_ALTERA)

include(${CONFIG_PATH}/version.cmake)
include(${CONFIG_PATH}/info.cmake)
#include(${CONFIG_PATH}/check.cmake)

if (WITH_DRM)
add_definitions(-DWITH_DRM)
endif()

if (IMX_G2D)
add_definitions(-DIMX_G2D)
endif()

if ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
set(LIST_INC
    dependent include
    ../utils3rd/3rd/jsoncpp/include/
    ${SDKTARGETSYSROOT}/usr/include
    ../utils3rd/3rd/include
    ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/include
    ../utils3rd/3rd/cpp-netlib/
    ../vvcam/common/
    ${UNITS_HEADER_FILES}
    #../units/cam_device/include
    ../mediacontrol/include_api
    ../appshell/display
    include/calibration
)
else ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
set(LIST_INC
    dependent include
    ../utils3rd/3rd/jsoncpp/include/
    ../utils3rd/3rd/tinyxml2
    ../utils3rd/3rd/include
    ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/include
    ../utils3rd/3rd/cpp-netlib/
    ../vvcam/common/
    ${UNITS_HEADER_FILES}
    #../units/cam_device/include
    ../mediacontrol/include_api
    ../appshell/display
    include/calibration
)
endif ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))

list(SORT LIST_INC)
include_directories(${LIST_INC})

#add_definitions(-DISP_MAX=0 -DISP_INPUT_MAX=1)
add_definitions(-DSERVER_ISP_PORT=0)

file(GLOB LIST_SRC_SERVER
    source/main.cpp
    source/network/*.cpp
    source/shell/*.cpp
    source/ioctl/*.cpp
    #source/calibdb/*.cpp
    #source/calibration/*.cpp
    source/common/*.cpp
    source/display/*.cpp
    dependent/base64/*.cpp
    )

if ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
link_directories(
  ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libebase.so
  ${SHELL_LIBS_DIRECTORY}
  ${SDKTARGETSYSROOT}/usr/lib/
    )
else ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
link_directories(
  ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libebase.so
  ${SHELL_LIBS_DIRECTORY}
    )
endif ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))

set(LIST_LIB_SERVER
    cppnetlib-client-connections
    cppnetlib-server-parsers
    cppnetlib-uri
    dl
    #exa_ctrl
    ${LIB_RT_NAME}
    ${LIB_PTHREAD_NAME}
    m
    jsoncpp_lib
    vvdisplay_shared
  )

# add exec to build env
add_executable(${application}
    ${LIST_SRC_SERVER}
    ${QM_FILES}
    ${GUI_QT_RESOURCES}
    ${SOURCES_COMMON}
    ${MOC_FILES_COMMON}
    ${MOC_FILES}
  )

# define lib dependencies
if(${FULL_SRC_COMPILE} STREQUAL 0)
    target_link_libraries(${application}
                    ${SHELL_LIBS_LIST}
                    ${LIST_LIB_SERVER}
    )
else(${FULL_SRC_COMPILE} STREQUAL 0)
if (GENERATE_PARTITION_BUILD)
    target_link_libraries(${application}
                    ${server_libs}
                    ${LIST_LIB_SERVER}
    )
    add_dependencies(${application} ${server_libs})
else (GENERATE_PARTITION_BUILD)
if ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
    target_link_libraries(${application}
					${SDKTARGETSYSROOT}/usr/lib/libtinyxml2.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libebase.so
                    ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/liboslayer.so
					${LIST_LIB_SERVER}
    )
else ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
    target_link_libraries(${application}
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libtinyxml2.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libebase.so
                    ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/liboslayer.so
					${LIST_LIB_SERVER}
    )
endif ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))

endif (GENERATE_PARTITION_BUILD)

endif(${FULL_SRC_COMPILE} STREQUAL 0)

# define stuff to install
install(TARGETS ${application} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

set(appsources ${SOURCES_COMMON}) # 'targets_app.cmake' requires this variable being set as input
include(${CMAKE_HOME_DIRECTORY}/targets_app.cmake)
