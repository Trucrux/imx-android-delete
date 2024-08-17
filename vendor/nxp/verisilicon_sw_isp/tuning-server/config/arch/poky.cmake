message("Load Poky...")

if(NOT ARCH)
  set(ARCH "-aarch64-poky-linux")
endif()

add_definitions(-DHAL_ALTERA)
