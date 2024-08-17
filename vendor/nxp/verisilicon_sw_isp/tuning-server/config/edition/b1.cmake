message("Load B1...")

include(${ARCH_PATH}/csky.cmake)
include(${CONFIG_PATH}/dependency.cmake)

add_definitions(-DEDITION_B1 -DISP_MAX=1 -DISP_INPUT_MAX=1)

add_definitions(-DCTRL_CNR -DCTRL_IE -DCTRL_WDR2)

set(PREVIEW OFF)
