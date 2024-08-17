cmake_minimum_required(VERSION 2.7.1)

set(PREVIEW OFF)
if (GENERATE_PARTITION_BUILD)
# define module name
set (module cam_device)

# we want to compile all .c files as default
file(
    GLOB libsources
    source/*.cpp
    source/calibdb/*.cpp
    source/calibration/*.cpp
    source/camera/*.cpp
    source/common/*.cpp
    source/commitf/*.cpp
	)

# set public headers, these get installed
file(
    GLOB pub_headers
    include/*.h
    include/*.hpp
	)

# define include paths
if((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64) AND (NOT ANDROID))
include_directories(
    include
    ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/include
    ../../utils3rd/3rd/jsoncpp/include
    ../../utils3rd/include
    ../../vvcam/common/
    ${SDKTARGETSYSROOT}/usr/include
    )
else ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64) AND (NOT ANDROID))
include_directories(
    include
    ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/include
    ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/include/tinyxml2
    ../../utils3rd/3rd/jsoncpp/include
    ../../utils3rd/include
    ../../vvcam/common/
    )
endif ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64) AND (NOT ANDROID))

# defines files to compile
file(GLOB SOURCES_COMMON source/*.cpp)


# define include paths
if((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64) AND (NOT ANDROID))
link_directories(
    ${LIBPFID_LIBRARY_DIRS}
    ${SDKTARGETSYSROOT}/usr/lib/
    )
else ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64) AND (NOT ANDROID))
link_directories(
    ${LIBPFID_LIBRARY_DIRS}
    )
endif ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (${PLATFORM} STREQUAL ARM64) AND (NOT ANDROID))

# module specific defines
add_definitions(-Wno-error=unused-function -DQT_NO_DEBUG_OUTPUT)

# add lib to build env
add_library(${module}_shared SHARED
            ${libsources}
           )

SET_TARGET_PROPERTIES(${module}_shared PROPERTIES OUTPUT_NAME     ${module})
SET_TARGET_PROPERTIES(${module}_shared PROPERTIES LINK_FLAGS      -shared)
SET_TARGET_PROPERTIES(${module}_shared PROPERTIES FRAMEWORK       TRUE PUBLIC_HEADER "${pub_headers}")

if ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (NOT ANDROID))
target_link_libraries(${module}_shared
                      cam_engine_shared # cam_engine will pull in ctrl & other libs as required ###${appl_libs}
                      mim_ctrl_shared
                      ${subctrl_libs}
                      ${ctrl_libs}
                      ${aaa_libs}
                      ${drv_libs}
                      ${platform_libs}
                      #jsoncpp_lib
                      base64_shared
                      tinyxml2
                      ${base_libs}
                      )
else ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (NOT ANDROID))
target_link_libraries(${module}_shared
                      cam_engine_shared # cam_engine will pull in ctrl & other libs as required ###${appl_libs}
                      mim_ctrl_shared
                      ${subctrl_libs}
                      ${ctrl_libs}
                      ${aaa_libs}
                      ${drv_libs}
                      ${platform_libs}
                      #jsoncpp_lib
                      base64_shared
                      tinyxml2_shared
                      ${base_libs}
                      )
endif ((${ISP_VERSION} STREQUAL ISP8000NANO_V1802) AND (NOT ANDROID))

# define stuff to install
install(TARGETS ${module}_shared
        PUBLIC_HEADER   DESTINATION ${CMAKE_INSTALL_PREFIX}/include/${module}
        ARCHIVE         DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${module}
        LIBRARY         DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${module}
        )

add_subdirectory(utils)

if(GENERATE_PARTITION_BUILD)
add_custom_target(copy_libs_${module} ALL
       COMMENT "##Copy cam_device static libs to propritories"
       COMMAND ${CMAKE_COMMAND} -E copy ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/lib${module}.so ${CMAKE_CURRENT_SOURCE_DIR}/proprietories/lib/lib${module}.so
       COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/include/${module} ${CMAKE_CURRENT_SOURCE_DIR}/proprietories/include
)
add_dependencies(copy_libs_${module} ${module}_shared)
endif(GENERATE_PARTITION_BUILD)

if( DEFINED APPSHELL_TOP_COMPILE)
add_custom_target(copy_shell_libs_${module} ALL
       COMMENT "##Copy libs to shell libs"
       COMMAND ${CMAKE_COMMAND} -E copy ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib/lib${module}.so ${CMAKE_HOME_DIRECTORY}/shell_libs/${APPMODE}/ispcore/${PLATFORM}/lib${module}.so
       COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/include/${module} ${CMAKE_HOME_DIRECTORY}/shell_libs/include/units_headers/${module}
)
add_dependencies(copy_shell_libs_${module} ${module}_shared)
endif( DEFINED APPSHELL_TOP_COMPILE)

# create common targets for this module
include(${UNITS_TOP_DIRECTORY}/targets.cmake)

else (GENERATE_PARTITION_BUILD)
add_subdirectory( proprietories )
add_subdirectory(utils)
endif (GENERATE_PARTITION_BUILD)