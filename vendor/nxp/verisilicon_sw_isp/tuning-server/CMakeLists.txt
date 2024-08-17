cmake_minimum_required(VERSION 2.6)

# define application name
set (application  tuning-server)

if(DEFINED PARTITION_BUILD)
    if(${PARTITION_BUILD} STREQUAL 0)
        set (GENERATE_PARTITION_BUILD 0)
    else(${PARTITION_BUILD} STREQUAL 0)
        set (GENERATE_PARTITION_BUILD 1)
    endif(${PARTITION_BUILD} STREQUAL 0)
else(DEFINED PARTITION_BUILD)
    #default value of GENERATE_PARTITION_BUILD
    set (GENERATE_PARTITION_BUILD 1)
endif(DEFINED PARTITION_BUILD)


if (CMAKE_BUILD_TYPE STREQUAL RELEASE)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DQT_NO_DEBUG_OUTPUT -DQT_NO_WARNING_OUTPUT")
endif (CMAKE_BUILD_TYPE STREQUAL RELEASE)

option(CTRL_DEWARP "Dewarp Control" OFF)
option(PREVIEW "Preview Support" ON)
option(SHELL_SIMULATE "Shell Simulate Mode" OFF)

if(${QTLESS} STREQUAL 0)
#QT env available
    set (QT_INCLUDE
        ${CMAKE_PREFIX_PATH}/include
        ${CMAKE_PREFIX_PATH}/include/QtCore
        ${CMAKE_PREFIX_PATH}/include/QtGui
        ${CMAKE_PREFIX_PATH}/include/QtMultimedia
        ${CMAKE_PREFIX_PATH}/include/QtWidgets
        ${CMAKE_PREFIX_PATH}/include/QtMultimediaWidgets
        ${QWT6_PATH}/include
    )

    set (QT_LIBS
        ${QT_LIBRARIES}
        qwt
        Qt5::Core
        Qt5::Gui
        Qt5::Qml 
        Qt5::Quick 
        Qt5::Multimedia 
        Qt5::MultimediaWidgets
        Qt5::Widgets
        Qt5::Xml
        qxcb
        )

    set (QT_DISPLAY_FULL_SRC_LIBS
        dom_ctrl_shared
        dom_ctrl_qt_shared
        )

    set(QT_DISPLAY_LIBS
        dom_ctrl dom_ctrl_qt 
        )

    set (QT_LINK_DIR
        ${QWT6_PATH}/lib
        ${CMAKE_PREFIX_PATH}/plugins/platforms
        )
    option(ISP_DOM "DOM Control" ON)

else(${QTLESS} STREQUAL 0)
#QT env does not available
set (QT_INCLUDE )
set (QT_LIBS )
set (QT_DISPLAY_LIBS )
set (QT_DISPLAY_FULL_SRC_LIBS )
set (QT_LINK_DIR )
option(ISP_DOM "DOM Control" OFF)
endif(${QTLESS} STREQUAL 0)

if(${FULL_SRC_COMPILE} STREQUAL 0)
    #link libs headers
    set(UNITS_HEADER_FILES 
        ${CMAKE_HOME_DIRECTORY}/shell_libs/include/units_headers/
        )

    set(SHELL_LIBS_DIRECTORY
        ${CMAKE_HOME_DIRECTORY}/shell_libs/${APPMODE}/ispcore/${PLATFORM}
    )

    set(SHELL_LIBS_LIST
    a2dnr a3dnr adpcc adpf aec aee aflt af ahdr avs awb awdr3 base64 bufferpool bufsync_ctrl cam_calibdb cam_device
    cam_engine cameric_drv cameric_reg_drv cim_ctrl common ebase fpga hal i2c_drv ibd isi mim_ctrl
    mipi_drv mom_ctrl oslayer som_ctrl tinyxml2 version vom_ctrl
    ${QT_DISPLAY_LIBS}
    )

else(${FULL_SRC_COMPILE} STREQUAL 0)
    #full src compile
    set(UNITS_HEADER_FILES )
    set(SHELL_LIBS_DIRECTORY )
    set(SHELL_LIBS_LIST )
endif(${FULL_SRC_COMPILE} STREQUAL 0)

if ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
set ( server_libs
    jsoncpp_lib ebase_shared vom_ctrl_shared som_ctrl_shared cam_engine_shared tinyxml2
    cameric_drv_shared cam_calibdb_shared cameric_reg_drv_shared fpga_shared hal_shared
    common_shared oslayer_shared isi_shared
    aec_shared awb_shared af_shared adpf_shared adpcc_shared a2dnr_shared a3dnr_shared awdr3_shared ahdr_shared aee_shared aflt_shared avs_shared
    bufferpool_shared mim_ctrl_shared mom_ctrl_shared
    version_shared mipi_drv_shared bufsync_ctrl_shared

    ${QT_DISPLAY_FULL_SRC_LIBS})
else ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
set ( server_libs
    jsoncpp_lib ebase_shared vom_ctrl_shared som_ctrl_shared cam_engine_shared tinyxml2_shared
    cameric_drv_shared cam_calibdb_shared cameric_reg_drv_shared fpga_shared hal_shared
    common_shared oslayer_shared isi_shared
    aec_shared awb_shared af_shared adpf_shared adpcc_shared a2dnr_shared a3dnr_shared awdr3_shared ahdr_shared aee_shared aflt_shared avs_shared
    bufferpool_shared mim_ctrl_shared mom_ctrl_shared
    version_shared mipi_drv_shared bufsync_ctrl_shared

    ${QT_DISPLAY_FULL_SRC_LIBS})
endif ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))

add_definitions(-DLINUX)
set(CONFIG_PATH config)

add_definitions(-DHAL_ALTERA)

include(${CONFIG_PATH}/version.cmake)
include(${CONFIG_PATH}/info.cmake)
#include(${CONFIG_PATH}/check.cmake)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
set(LIST_INC
    dependent include
    ../utils3rd/3rd/jsoncpp/include/
    ${SDKTARGETSYSROOT}/usr/include
    ../utils3rd/3rd/include
    ../vvcam/common/
    ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/include
    ../utils3rd/3rd/cpp-netlib/
    ${QT_INCLUDE}
    ${UNITS_HEADER_FILES}
    ../appshell/display
    ../appshell/yuv
    ../mediacontrol/buffer
    ../mediacontrol/include
)
else ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
set(LIST_INC
    dependent include
    ../utils3rd/3rd/jsoncpp/include/
    ../utils3rd/3rd/tinyxml2
    ../utils3rd/3rd/include
    ../vvcam/common/
    ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/include
    ../utils3rd/3rd/cpp-netlib/
    ${QT_INCLUDE}
    ${UNITS_HEADER_FILES}
    ../appshell/display
    ../appshell/yuv
    ../mediacontrol/buffer
    ../mediacontrol/include
)
endif ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))

list(SORT LIST_INC)
include_directories(${LIST_INC})

add_definitions(-DISP_MAX=1 -DISP_INPUT_MAX=1)
add_definitions(-DSERVER_ISP_PORT=0)

if(CTRL_DEWARP)
  add_definitions(-DCTRL_DEWARP)
endif()

file(GLOB LIST_SRC_SERVER source/*.cpp source/network/*.cpp
    source/shell/*.cpp
    dependent/base64/*.cpp
    source/calibdb/*.cpp
    source/calibration/*.cpp
    source/camera/*.cpp
    source/common/*.cpp
    #source/exa/*.cpp
    source/som/*.cpp
    source/vom/*.cpp
    )

if(ISP_DOM)
list(APPEND LIST_SRC_SERVER source/dom/dom.cpp)
add_definitions(-DISP_DOM)
endif()

if (WITH_DRM)
list(APPEND LIST_SRC_SERVER source/drm/drm.cpp)
add_definitions(-DISP_DRM)
set (drm_link_libs yuv2rgb_static media_buffer_static vvdisplay_shared)
else(WITH_DRM)
set (drm_link_libs )
set (CMAKE_CXX_FLAGS_DEBUG "-fno-strict-aliasing -O3")
set (CMAKE_C_FLAGS_DEBUG "-fno-strict-aliasing -O3")
endif()

if ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
link_directories(
  ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib
  ${QT_LINK_DIR}
  ${SHELL_LIBS_DIRECTORY}
  ${SDKTARGETSYSROOT}/usr/lib/
    )
else ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
link_directories(
  ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib
  ${QT_LINK_DIR}
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
    ${QT_LIBS}
    jsoncpp_lib
    ${drm_link_libs}
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
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libcam_device.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libcam_engine.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libcameric_drv.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libhal.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libbase64.so
					${SDKTARGETSYSROOT}/usr/lib/libtinyxml2.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libbufferpool.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libcameric_reg_drv.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libisi.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libmipi_drv.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libcam_calibdb.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libfpga.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libebase.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/liboslayer.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libcommon.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libaec.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libawb.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libaf.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libadpf.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libadpcc.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/liba2dnr.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/liba3dnr.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libawdr3.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libahdr.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libaee.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libaflt.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libavs.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libmim_ctrl.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libmom_ctrl.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libbufsync_ctrl.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libsom_ctrl.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libversion.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libvom_ctrl.so
					${LIST_LIB_SERVER}
    )
else ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))
    target_link_libraries(${application}
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libcam_device.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libcam_engine.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libcameric_drv.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libhal.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libbase64.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libtinyxml2.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libbufferpool.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libcameric_reg_drv.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libisi.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libmipi_drv.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libcam_calibdb.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libfpga.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libebase.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/liboslayer.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libcommon.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libaec.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libawb.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libaf.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libadpf.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libadpcc.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/liba2dnr.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/liba3dnr.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libawdr3.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libahdr.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libaee.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libaflt.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libavs.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libmim_ctrl.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libmom_ctrl.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libbufsync_ctrl.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libsom_ctrl.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libversion.so
					${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/libvom_ctrl.so
					${LIST_LIB_SERVER}
    )
endif ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64))

endif (GENERATE_PARTITION_BUILD)

endif(${FULL_SRC_COMPILE} STREQUAL 0)

# define stuff to install
install(TARGETS ${application} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

set(appsources ${SOURCES_COMMON}) # 'targets_app.cmake' requires this variable being set as input
include(${CMAKE_HOME_DIRECTORY}/targets_app.cmake)


