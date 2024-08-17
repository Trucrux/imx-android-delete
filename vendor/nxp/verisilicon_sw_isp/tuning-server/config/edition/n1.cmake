message("Load N1...")

include(${ARCH_PATH}/x86_64.cmake)
include(${CONFIG_PATH}/dependency.cmake)

add_definitions(-DEDITION_N1 -DISP_MAX=1 -DISP_INPUT_MAX=2)

add_definitions(-DCTRL_CNR -DCTRL_HDR -DCTRL_WDR3)

set(CTRL_DEWARP ON)
