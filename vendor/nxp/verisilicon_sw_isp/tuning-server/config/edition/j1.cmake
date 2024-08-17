message("Load J1...")

include(${ARCH_PATH}/x86_64.cmake)
include(${CONFIG_PATH}/dependency.cmake)

add_definitions(-DEDITION_J1 -DISP_MAX=1 -DISP_INPUT_MAX=1)

add_definitions(
  -DCTRL_2DNR
  -DCTRL_3DNR
  -DCTRL_AVS
  -DCTRL_CNR
  -DCTRL_DEHAZE
  -DCTRL_EE
  -DCTRL_HDR
  -DCTRL_SELF_PATH_2
  -DCTRL_WDR3)

set(CTRL_DEWARP ON)
