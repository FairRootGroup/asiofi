################################################################################
# Copyright (C) 2018-2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
cmake_policy(VERSION 3.15...3.20)

# Project ######################################################################
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(GitHelper)
get_git_version()

project(asiofi VERSION ${PROJECT_VERSION} LANGUAGES CXX)

include(CTest)
include(${PROJECT_NAME}ProjectSettings)
################################################################################


# Build options ################################################################
include(${PROJECT_NAME}BuildOption)
build_option(BUILD_CORE "Build asiofi library." DEFAULT ON)
build_option(BUILD_TESTING "Build tests." DEFAULT OFF REQUIRES "BUILD_CORE")
################################################################################


# Dependencies #################################################################
include(FairCMakeModules)
include(FairFindPackage2)
include(asiofiBundlePackageHelper)

if(BUILD_CORE)
  find_package2(PUBLIC Threads REQUIRED)
  set(Threads_PREFIX "<unknown system prefix>")

  find_package2(PUBLIC OFI VERSION 1.6.0 REQUIRED COMPONENTS fi_sockets)

  find_package2(PUBLIC asio VERSION 1.18 REQUIRED)
endif()

if(BUILD_TESTING)
  find_package2(PRIVATE CLI11 VERSION 1.9)
  if(NOT CLI11_FOUND AND NOT CLI11_BUNDLED)
    build_bundled(CLI11 extern/CLI11)
    find_package2(PRIVATE CLI11 REQUIRED)
  endif()
endif()

find_package2_implicit_dependencies()
################################################################################


# Targets ######################################################################
if(BUILD_CORE)
  # Configure Version.hpp
  configure_file(include/${PROJECT_NAME_LOWER}/version.hpp.in
    ${CMAKE_BINARY_DIR}/include/${PROJECT_NAME_LOWER}/version.hpp
    @ONLY
  )

  # Define the library
  set(target asiofi)
  add_library(${target} INTERFACE)
  target_link_libraries(${target}
    INTERFACE
    Threads::Threads
    OFI::libfabric
    asio::asio
  )
  target_compile_features(${target} INTERFACE cxx_std_17)
  target_include_directories(${target} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
endif()

# Add fabtests
if(BUILD_TESTING)
  add_subdirectory(test)
endif()
################################################################################


# Package components ###########################################################
if(BUILD_CORE)
  list(APPEND PROJECT_PACKAGE_COMPONENTS core)
endif()
if(BUILD_TESTING)
  list(APPEND PROJECT_PACKAGE_COMPONENTS tests)
endif()
################################################################################


# Installation #################################################################
if(BUILD_CORE)
  install(TARGETS asiofi
    EXPORT ${PROJECT_EXPORT_SET}
    DESTINATION ${PROJECT_INSTALL_LIBDIR}
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
  install(FILES cmake/FindOFI.cmake
    DESTINATION ${PROJECT_INSTALL_CMAKEMODDIR}
  )

  include(asiofiPackage)
  install_cmake_package()
endif()
################################################################################


# Summary ######################################################################
include(asiofiSummary)

message(STATUS "${BWhite}${PROJECT_NAME}${CR} ${PROJECT_GIT_VERSION} from ${PROJECT_DATE}")
fair_summary_global_cxx_flags_standard()
fair_summary_build_types()
fair_summary_package_dependencies()
asiofi_summary_components()
asiofi_summary_install_prefix()
message(STATUS "  ")
################################################################################

