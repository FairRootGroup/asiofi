################################################################################
#    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

cmake_minimum_required(VERSION 3.10 FATAL_ERROR)


# Project ######################################################################
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(asiofiLib)

set_asiofi_cmake_policies()
get_git_version(OUTVAR_PREFIX asiofi)

project(asiofi VERSION ${asiofi_VERSION} LANGUAGES CXX)
message("${BWhite}${PROJECT_NAME}${CR} ${asiofi_GIT_VERSION} from ${asiofi_DATE}")

set_asiofi_defaults()
################################################################################


# Build options ################################################################
option(BUILD_FABTESTS "Build fabtests." OFF)
################################################################################


# Dependencies #################################################################
find_package2(PUBLIC Boost VERSION 1.66 REQUIRED
  COMPONENTS system program_options container)
find_package2(PUBLIC OFI VERSION 1.6.0 REQUIRED
  COMPONENTS fi_sockets)

if(BUILD_FABTESTS)
  if(BENCHMARK_ROOT)
    set(CMAKE_PREFIX_PATH ${BENCHMARK_ROOT} ${CMAKE_PREFIX_PATH})
  endif()
  find_package2(PRIVATE benchmark REQUIRED)
endif()
################################################################################


# Targets ######################################################################
# Configure Version.hpp
configure_file(include/${PROJECT_NAME_LOWER}/version.hpp.in
  ${CMAKE_BINARY_DIR}/include/${PROJECT_NAME_LOWER}/version.hpp
  @ONLY
)

# Define the library
add_library(asiofi INTERFACE)
target_link_libraries(asiofi
  INTERFACE Boost::boost Boost::system OFI::libfabric
)
target_include_directories(asiofi INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Add fabtests
if(BUILD_FABTESTS)
  add_subdirectory(fabtests)
endif()
################################################################################


# Package components ###########################################################
list(APPEND PROJECT_PACKAGE_COMPONENTS bindings)
if(BUILD_FABTESTS)
  list(APPEND PROJECT_PACKAGE_COMPONENTS fabtests)
endif()
################################################################################


# Installation #################################################################
install(TARGETS asiofi
  EXPORT ${asiofi_EXPORT_SET}
  DESTINATION ${asiofi_INSTALL_LIBDIR}
)
install(DIRECTORY
  include/${PROJECT_NAME_LOWER}
  ${CMAKE_BINARY_DIR}/include/${PROJECT_NAME_LOWER}

  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PATTERN *.in EXCLUDE
)
install(FILES include/asiofi.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install_asiofi_cmake_package()
################################################################################


# Summary ######################################################################
if(PROJECT_PACKAGE_DEPENDENCIES)
  message(STATUS "  ")
  message(STATUS "  ${Cyan}DEPENDENCY FOUND     VERSION                   PREFIX${CR}")
  foreach(dep IN LISTS PROJECT_PACKAGE_DEPENDENCIES)
    if(${dep}_VERSION)
      if(${dep} STREQUAL Boost)
        set(version_str "${BGreen}${${dep}_MAJOR_VERSION}.${${dep}_MINOR_VERSION}${CR}")
      else()
        set(version_str "${BGreen}${${dep}_VERSION}${CR}")
      endif()
    else()
      set(version_str "${BYellow}unknown${CR}")
    endif()
    if(PROJECT_${dep}_VERSION)
      set(version_req_str " (>= ${PROJECT_${dep}_VERSION})")
    endif()
    pad(${dep} 20 " " dep_padded)
    if(DISABLE_COLOR)
      pad("${version_str}${version_req_str}" 25 " " version_padded)
    else()
      pad("${version_str}${version_req_str}" 25 " " version_padded COLOR 1)
    endif()
    if(${dep} STREQUAL OFI)
      get_filename_component(prefix ${${dep}_INCLUDE_DIRS}/.. ABSOLUTE)
    elseif(${dep} STREQUAL benchmark)
      get_target_property(benchmark_lib benchmark::benchmark INTERFACE_LOCATION)
      get_filename_component(prefix ${benchmark_lib} DIRECTORY)
      get_filename_component(prefix ${prefix}/.. ABSOLUTE)
    else()
      get_filename_component(prefix ${${dep}_INCLUDE_DIR}/.. ABSOLUTE)
    endif()
    message(STATUS "  ${BWhite}${dep_padded}${CR}${version_padded}${prefix}")
    unset(version_str)
    unset(version_padded)
    unset(version_req_str)
  endforeach()
endif()
message(STATUS "  ")
message(STATUS "  ${Cyan}COMPONENT  BUILT?  INFO${CR}")
message(STATUS "  ${BWhite}bindings${CR}    ${BGreen}YES${CR}    (default, always built)")
if(BUILD_FABTESTS)
  set(fabtests_summary "${BGreen}YES${CR}    (disable with ${BMagenta}-DBUILD_FABTESTS=OFF${CR})")
else()
  set(fabtests_summary "${BRed} NO${CR}    (default, enable with ${BMagenta}-DBUILD_FABTESTS=ON${CR})")
endif()
message(STATUS "  ${BWhite}fabtests${CR}    ${fabtests_summary}")
message(STATUS "  ")
################################################################################
