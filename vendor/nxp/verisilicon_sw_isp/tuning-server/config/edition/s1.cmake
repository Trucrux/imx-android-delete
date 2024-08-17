message("Load S1...")

include(${ARCH_PATH}/x86_64.cmake)
include(${CONFIG_PATH}/dependency.cmake)

add_definitions(-DEDITION_S1 -DISP_MAX=1 -DISP_INPUT_MAX=1)

add_definitions(-DCTRL_3DNR -DCTRL_CNR -DCTRL_HDR -DCTRL_IE -DCTRL_SELF_PATH_2
                -DCTRL_WDR3)
