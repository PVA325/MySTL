cmake_minimum_required(VERSION 3.25)
project(MySTL)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(MySTL INTERFACE)
add_library(MySTL::MySTL ALIAS MySTL)

target_include_directories(
  MySTL INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
  TARGETS MySTL
  EXPORT MySTLTargets
)

install(
  EXPORT MySTLTargets
  NAMESPACE MySTL::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MySTL
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/MySTLConfigVersion.cmake
  VERSION 1.0.0
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MySTLConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/MySTLConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MySTL
)

install(
  FILES
  ${CMAKE_CURRENT_BINARY_DIR}/MySTLConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/MySTLConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MySTL
)

