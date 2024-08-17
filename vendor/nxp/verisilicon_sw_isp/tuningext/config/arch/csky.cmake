message("Load CSky...")

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR csky)

set(TOOLS /opt/csky-linux-gnuabiv2-tools)
set(PREFIX csky-abiv2-linux-)
set(CMAKE_C_COMPILER ${TOOLS}/bin/${PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLS}/bin/${PREFIX}g++)

set(CMAKE_C_FLAGS
    "-Wall -Wextra -std=c99 -Wformat-nonliteral -mfloat-abi=hard -mcpu=ck860f"
    CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_DEBUG
    "-g -O0 -DDEBUG -mfloat-abi=hard -mcpu=ck860f"
    CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE
    "-fno-strict-aliasing -O3 -mfloat-abi=hard -mcpu=ck860f"
    CACHE STRING "" FORCE)

set(CMAKE_CXX_FLAGS
    "-Wall -Wextra -std=c++0x -Wformat-nonliteral -mfloat-abi=hard -mcpu=ck860f"
    CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG
    "-g -O0 -DDEBUG -mfloat-abi=hard -mcpu=ck860f"
    CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE
    "-fno-strict-aliasing -O3 -mfloat-abi=hard -mcpu=ck860f"
    CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS
    "-mfloat-abi=hard -mcpu=ck860f"
    CACHE STRING "" FORCE)

if(NOT ARCH)
  set(ARCH "-csky")
endif()

if(NOT BOOST_SOURCE_ROOT)
  get_filename_component(BOOST_SOURCE_ROOT ../boost_1_69_0 ABSOLUTE)
else()
  get_filename_component(BOOST_SOURCE_ROOT ${BOOST_SOURCE_ROOT} ABSOLUTE)
endif()

if(NOT BOOST_BUILD_ROOT)
  get_filename_component(
    BOOST_BUILD_ROOT ../build-boost_1_69_0${ARCH}-${CMAKE_BUILD_TYPE} ABSOLUTE)
else()
  get_filename_component(BOOST_BUILD_ROOT ${BOOST_BUILD_ROOT} ABSOLUTE)
endif()

add_definitions(
  -DHAL_ALTERA -DBOOST_ASIO_DISABLE_STD_FUTURE
  # -DUSE_STANDARD_ASSERT
)
