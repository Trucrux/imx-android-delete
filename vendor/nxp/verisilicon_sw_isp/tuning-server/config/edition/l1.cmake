message("Load L1...")

include(${ARCH_PATH}/x86_64.cmake)
include(${CONFIG_PATH}/dependency.cmake)

add_definitions(-DEDITION_L1 -DISP_MAX=1 -DISP_INPUT_MAX=1)

add_definitions(-DCTRL_AVS)
