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
include(AsiofiLib)

set_asiofi_cmake_policies()
get_git_version(OUTVAR_PREFIX Asiofi)

project(Asiofi VERSION ${Asiofi_VERSION} LANGUAGES C CXX)
message("${BWhite}${PROJECT_NAME}${CR} ${Asiofi_GIT_VERSION} from ${Asiofi_DATE}")

set_asiofi_defaults()
################################################################################


# Build options ################################################################
option(BUILD_FABTESTS "Build fabtests." OFF)
################################################################################


# Dependencies #################################################################
if(Boost_INCLUDE_DIR) # Silence output, if Boost was found already
  set(SILENCE_BOOST QUIET)
endif()
find_package(Boost 1.64 ${SILENCE_BOOST} REQUIRED COMPONENTS system)
find_package(OFI 1.6.0 REQUIRED COMPONENTS fi_sockets)

if(BUILD_FABTESTS)
  if(BENCHMARK_ROOT)
    set(CMAKE_PREFIX_PATH ${BENCHMARK_ROOT} ${CMAKE_PREFIX_PATH})
  endif()
  find_package(benchmark REQUIRED)
  # find_package(FlatBuffers ...)
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


# Installation #################################################################
install(TARGETS asiofi
  EXPORT ${Asiofi_EXPORT_SET}
  DESTINATION ${Asiofi_INSTALL_LIBDIR}
)
install(DIRECTORY
  include/${PROJECT_NAME_LOWER}
  ${CMAKE_BINARY_DIR}/include/${PROJECT_NAME_LOWER}

  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PATTERN *.in EXCLUDE
)

install_asiofi_cmake_package()
################################################################################


# Summary ######################################################################
message("  ")
message("  COMPONENT  BUILT?  INFO")
message("  ${BWhite}bindings${CR}    ${BGreen}YES${CR}    (default, always built)")
if(BUILD_FABTESTS)
  set(fabtests_summary "${BGreen}YES${CR}    (disable with ${BMagenta}-DBUILD_FABTESTS=OFF${CR})")
else()
  set(fabtests_summary "${BRed} NO${CR}    (default, enable with ${BMagenta}-DBUILD_FABTESTS=ON${CR})")
endif()
message("  ${BWhite}fabtests${CR}    ${fabtests_summary}")
message("  ")
################################################################################
