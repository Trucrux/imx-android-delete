message("Load x86_64...")

set(CMAKE_C_FLAGS "-Wall -Wextra -std=c99")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-fno-strict-aliasing -O3")

set(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++0x")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-fno-strict-aliasing -O3")

if(NOT ARCH)
  set(ARCH "-ubuntu_16_04_x86_64")
endif()

add_definitions(-DHAL_ALTERA)
