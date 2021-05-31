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

if(BUILD_CORE)
  find_package2(PUBLIC Threads REQUIRED)
  set(Threads_PREFIX "<unknown system prefix>")

  # workaround bug https://github.com/boostorg/boost_install/issues/5
  set(__org ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS ON)
  find_package2(PUBLIC Boost VERSION 1.70 REQUIRED
    COMPONENTS program_options container)
  set(BUILD_SHARED_LIBS ${__org})
  find_package2(PUBLIC OFI VERSION 1.6.0 REQUIRED
    COMPONENTS fi_sockets)

  find_package2(PUBLIC folly VERSION 2019.04.01.00)
  if (folly_FOUND)
    set(FOLLY_EXTERN Folly::folly)
    unset(FOLLY_INTERN)
  else()
    # Could not find an external installation, falling back to internal (light) copy
    unset(FOLLY_EXTERN)
    set(FOLLY_INTERN folly)
    add_library(folly INTERFACE)
    target_include_directories(folly INTERFACE
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/asiofi/3rdparty>
      $<INSTALL_INTERFACE:include/asiofi/3rdparty>
    )
    target_compile_definitions(folly INTERFACE FOLLY_NO_CONFIG)
    set(PROJECT_folly_VERSION 2019.04.01.00)
    set(folly_VERSION internal)
    set(PROJECT_PACKAGE_DEPENDENCIES ${PROJECT_PACKAGE_DEPENDENCIES} folly)
  endif ()
  set(FOLLY_DEP ${FOLLY_EXTERN} ${FOLLY_INTERN})
endif()
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
    Boost::boost
    Threads::Threads
    Boost::container
    OFI::libfabric
    ${FOLLY_DEP}
  )
  target_compile_features(${target} INTERFACE cxx_std_17)
  target_include_directories(${target} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  target_compile_definitions(${target} INTERFACE BOOST_ERROR_CODE_HEADER_ONLY)

  # workaround https://github.com/boostorg/asio/commit/43874d5497414c67655d901e48c939ef01337edb
  if (    ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
      AND ("${CMAKE_CXX_COMPILER_VERSION}" VERSION_GREATER "10.0.0"))
    if ("${Boost_VERSION}" VERSION_LESS "106900")
      target_compile_definitions(${target} INTERFACE
        BOOST_ASIO_HAS_STD_STRING_VIEW)
    endif()
  endif()
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
  install(TARGETS asiofi ${FOLLY_INTERN}
    EXPORT ${PROJECT_EXPORT_SET}
    DESTINATION ${PROJECT_INSTALL_LIBDIR}
  )
  install(DIRECTORY
    include/${PROJECT_NAME_LOWER}
    ${CMAKE_BINARY_DIR}/include/${PROJECT_NAME_LOWER}

    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PATTERN *.in EXCLUDE
    PATTERN 3rdparty EXCLUDE
  )
  install(FILES include/asiofi.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
  if (FOLLY_INTERN)
    install(DIRECTORY include/${PROJECT_NAME_LOWER}/3rdparty
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME_LOWER}
    )
  endif()
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

