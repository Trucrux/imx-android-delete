message("Load Dependant...")

if(NOT BOOST_SOURCE_ROOT)
  get_filename_component(BOOST_SOURCE_ROOT ../boost_1_71_0 ABSOLUTE)
else()
  get_filename_component(BOOST_SOURCE_ROOT ${BOOST_SOURCE_ROOT} ABSOLUTE)
endif()

if(NOT BOOST_BUILD_ROOT)
  get_filename_component(
    BOOST_BUILD_ROOT ../build/build-boost_1_71_0${ARCH}-${CMAKE_BUILD_TYPE}
    ABSOLUTE)
else()
  get_filename_component(BOOST_BUILD_ROOT ${BOOST_BUILD_ROOT} ABSOLUTE)
endif()

if(NOT CPPNETLIB_SOURCE_ROOT)
  get_filename_component(CPPNETLIB_SOURCE_ROOT ../cpp-netlib-master ABSOLUTE)
else()
  get_filename_component(CPPNETLIB_SOURCE_ROOT ${CPPNETLIB_SOURCE_ROOT}
                         ABSOLUTE)
endif()

if(NOT CPPNETLIB_BUILD_ROOT)
  get_filename_component(
    CPPNETLIB_BUILD_ROOT
    ../build/build-cpp-netlib-master${ARCH}-${CMAKE_BUILD_TYPE} ABSOLUTE)
else()
  get_filename_component(CPPNETLIB_BUILD_ROOT ${CPPNETLIB_BUILD_ROOT} ABSOLUTE)
endif()

if(NOT DEWARP_SOURCE_ROOT)
  get_filename_component(DEWARP_SOURCE_ROOT ../dewarp ABSOLUTE)
else()
  get_filename_component(DEWARP_SOURCE_ROOT ${DEWARP_SOURCE_ROOT} ABSOLUTE)
endif()

if(NOT DEWARP_BUILD_ROOT)
  get_filename_component(
    DEWARP_BUILD_ROOT ../build/build-dewarp${ARCH}-${CMAKE_BUILD_TYPE} ABSOLUTE)
else()
  get_filename_component(DEWARP_BUILD_ROOT ${DEWARP_BUILD_ROOT} ABSOLUTE)
endif()

if(NOT JSONCPP_SOURCE_ROOT)
  get_filename_component(JSONCPP_SOURCE_ROOT ../jsoncpp-master ABSOLUTE)
else()
  get_filename_component(JSONCPP_SOURCE_ROOT ${JSONCPP_SOURCE_ROOT} ABSOLUTE)
endif()

if(NOT JSONCPP_BUILD_ROOT)
  get_filename_component(
    JSONCPP_BUILD_ROOT ../build/build-jsoncpp-master${ARCH}-${CMAKE_BUILD_TYPE}
    ABSOLUTE)
else()
  get_filename_component(JSONCPP_BUILD_ROOT ${JSONCPP_BUILD_ROOT} ABSOLUTE)
endif()

if(NOT MEDIACONTROL_SOURCE_ROOT)
  get_filename_component(MEDIACONTROL_SOURCE_ROOT ../mediacontrol ABSOLUTE)
else()
  get_filename_component(MEDIACONTROL_SOURCE_ROOT ${MEDIACONTROL_SOURCE_ROOT}
                         ABSOLUTE)
endif()

if(NOT MEDIACONTROL_BUILD_ROOT)
  get_filename_component(
    MEDIACONTROL_BUILD_ROOT
    ../build/build-mediacontrol${ARCH}-${CMAKE_BUILD_TYPE} ABSOLUTE)
else()
  get_filename_component(MEDIACONTROL_BUILD_ROOT ${MEDIACONTROL_BUILD_ROOT}
                         ABSOLUTE)
endif()

if(NOT TINYXML2_SOURCE_ROOT)
  get_filename_component(TINYXML2_SOURCE_ROOT ../tinyxml2-master ABSOLUTE)
else()
  get_filename_component(TINYXML2_SOURCE_ROOT ${TINYXML2_SOURCE_ROOT} ABSOLUTE)
endif()

if(NOT TINYXML2_BUILD_ROOT)
  get_filename_component(
    TINYXML2_BUILD_ROOT
    ../build/build-tinyxml2-master${ARCH}-${CMAKE_BUILD_TYPE} ABSOLUTE)
else()
  get_filename_component(TINYXML2_BUILD_ROOT ${TINYXML2_BUILD_ROOT} ABSOLUTE)
endif()

if(NOT UNITS_GENERATE_ROOT)
  get_filename_component(UNITS_GENERATE_ROOT ${LIB_ROOT} ABSOLUTE)
else()
  get_filename_component(UNITS_GENERATE_ROOT ${UNITS_GENERATE_ROOT} ABSOLUTE)
endif()
