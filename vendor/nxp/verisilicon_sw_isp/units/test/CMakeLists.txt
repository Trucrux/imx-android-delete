cmake_minimum_required(VERSION 2.6)
if (GENERATE_PARTITION_BUILD)
set(APP isp_test)

set(BARE_METAL bare_metal)
set(ENGINE engine)
set(FILE file)
set(JSON json)

# include directories
include_directories(
    include
    include/${BARE_METAL}
    include/${ENGINE}
    include/${JSON}
    include/${FILE}
    ../cam_engine/include
    ../cam_engine/include_priv
    ../cameric_drv/include
    ../cameric_drv/include_priv
    ../cam_calibdb/include
    ../cam_calibdb/include_priv
    ../../vvcam/isp/
    ../../vvcam/common/
    ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/include
    )

file(GLOB BARE_METAL_SRC src/${BARE_METAL}/*.c)
file(GLOB CAM_ENGINE_SRC src/${ENGINE}/*.c src/${ENGINE}/parse/*.c)
file(GLOB FILE_SRC src/${FILE}/*.c)
file(GLOB JSON_SRC src/${JSON}/*.c)
file(GLOB TEST_SRC src/*.c)

#add_definitions(-D_DEFAULT_SOURCE -DCHIP_8000L) # {CHIP_8000L, CHIP_NANO}

add_executable(${APP}
    ${TEST_SRC}
    ${BARE_METAL_SRC}
    ${CAM_ENGINE_SRC}
    ${FILE_SRC}
    ${JSON_SRC}
)

link_directories(
    ${LIB_ROOT}/${CMAKE_BUILD_TYPE}/lib
)


target_link_libraries(
    ${APP}
    cam_engine_shared
    som_ctrl_shared
    ${drv_libs}
    ${aaa_libs}
    ${ctrl_libs}
    ${base_libs}
    ${platform_libs}
    ${LIB_PTHREAD_NAME}
    isi
    m
)

install(TARGETS ${APP} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif (GENERATE_PARTITION_BUILD)
