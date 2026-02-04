cmake_minimum_required(VERSION 3.16)
project(gst_udpjson_meta LANGUAGES CXX)

set(LIB_INSTALL_DIR "/opt/nvidia/deepstream/deepstream/lib" CACHE PATH "Library install dir")
set(GST_INSTALL_DIR "/opt/nvidia/deepstream/deepstream/lib/gst-plugins/" CACHE PATH "GStreamer plugin dir")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fPIC")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-base-1.0)
pkg_check_modules(JSONGLIB REQUIRED json-glib-1.0)

add_library(gst_udpjson_meta SHARED gstudpjsonmeta.cpp gstudpjsonmeta_cuav.cpp)

target_include_directories(gst_udpjson_meta PRIVATE
  /opt/nvidia/deepstream/deepstream/sources/includes
  /usr/include/gstreamer-1.0
  ${GST_INCLUDE_DIRS}
  ${JSONGLIB_INCLUDE_DIRS}
  ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(gst_udpjson_meta PRIVATE
  ${GST_LIBRARIES}
  ${JSONGLIB_LIBRARIES}
  -L${LIB_INSTALL_DIR} -lnvdsgst_helper -lnvdsgst_meta -lnvds_meta
)

target_link_options(gst_udpjson_meta PRIVATE "-Wl,-rpath,${LIB_INSTALL_DIR}")
target_link_options(gst_udpjson_meta PRIVATE "-Wl,-no-undefined")

set_target_properties(gst_udpjson_meta
  PROPERTIES
    OUTPUT_NAME "udpjsonmeta"
    POSITION_INDEPENDENT_CODE ON
    INSTALL_RPATH ${LIB_INSTALL_DIR}
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

install(TARGETS gst_udpjson_meta LIBRARY DESTINATION ${LIB_INSTALL_DIR})
install(TARGETS gst_udpjson_meta LIBRARY DESTINATION ${GST_INSTALL_DIR})
