message("Load Edition...")

set(ARCH_PATH ${CONFIG_PATH}/arch)

set(CTRL_DEWARP OFF)
set(PREVIEW ON)
include(${CONFIG_PATH}/edition/${EDITION}.cmake)
