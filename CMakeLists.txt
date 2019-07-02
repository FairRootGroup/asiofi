################################################################################
#    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
cmake_policy(VERSION 3.10...3.14)

# Project ######################################################################
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(asiofiLib)

get_git_version(OUTVAR_PREFIX asiofi)

project(asiofi VERSION ${asiofi_VERSION} LANGUAGES CXX)
message("${BWhite}${PROJECT_NAME}${CR} ${asiofi_GIT_VERSION} from ${asiofi_DATE}")

set_asiofi_defaults()
include(CTest)
################################################################################


# Build options ################################################################
include(CMakeDependentOption)
option(BUILD_CORE "Build asiofi library." ON)
cmake_dependent_option(BUILD_TESTING "Build tests." OFF "BUILD_CORE" OFF)
################################################################################


# Dependencies #################################################################
if(BUILD_CORE)
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  find_package(Threads REQUIRED)

  find_package2(PUBLIC Boost VERSION 1.70 REQUIRED
    COMPONENTS program_options container)
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
  add_library(asiofi INTERFACE)
  target_link_libraries(asiofi
    INTERFACE
    Boost::boost
    Threads::Threads
    Boost::container
    OFI::libfabric
    ${FOLLY_DEP}
  )
  target_include_directories(asiofi INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  target_compile_definitions(asiofi INTERFACE BOOST_ERROR_CODE_HEADER_ONLY)

  # workaround https://github.com/boostorg/asio/commit/43874d5497414c67655d901e48c939ef01337edb
  if (    ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
      AND ("${CMAKE_CXX_COMPILER_VERSION}" VERSION_GREATER "10.0.0"))
    if ("${Boost_VERSION}" VERSION_LESS "106900")
      target_compile_definitions(asiofi INTERFACE
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
    EXPORT ${asiofi_EXPORT_SET}
    DESTINATION ${asiofi_INSTALL_LIBDIR}
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

  install_asiofi_cmake_package()
endif()
################################################################################


# Summary ######################################################################
if(PROJECT_PACKAGE_DEPENDENCIES)
  message(STATUS "  ")
  message(STATUS "  ${Cyan}DEPENDENCY FOUND   VERSION                      PREFIX${CR}")
  foreach(dep IN LISTS PROJECT_PACKAGE_DEPENDENCIES)
    if(${dep} STREQUAL Boost)
      if(DEFINED Boost_VERSION_STRING)
        set(_version "${Boost_VERSION_STRING}")
      elseif(DEFINED Boost_MAJOR_VERSION)
        set(_version "${Boost_MAJOR_VERSION}.${Boost_MINOR_VERSION}.${Boost_SUBMINOR_VERSION}")
      else()
        set(_version "${Boost_VERSION}")
      endif()
      set(version_str "${BGreen}${_version}${CR}")
    elseif(${dep}_VERSION)
      set(version_str "${BGreen}${${dep}_VERSION}${CR}")
    else()
      set(version_str "${BYellow}unknown${CR}")
    endif()
    if(PROJECT_${dep}_VERSION)
      set(version_req_str " (>= ${PROJECT_${dep}_VERSION})")
    endif()
    pad(${dep} 18 " " dep_padded)
    if(DISABLE_COLOR)
      pad("${version_str}${version_req_str}" 28 " " version_padded)
    else()
      pad("${version_str}${version_req_str}" 28 " " version_padded COLOR 1)
    endif()
    if(${dep} STREQUAL OFI)
      get_filename_component(prefix ${${dep}_INCLUDE_DIRS}/.. ABSOLUTE)
    elseif(${dep} STREQUAL Boost)
      get_target_property(prefix Boost::boost INTERFACE_INCLUDE_DIRECTORIES)
      get_filename_component(prefix ${prefix}/.. ABSOLUTE)
    elseif(${dep} STREQUAL benchmark)
      get_target_property(benchmark_lib benchmark::benchmark INTERFACE_LOCATION)
      get_filename_component(prefix ${benchmark_lib} DIRECTORY)
      get_filename_component(prefix ${prefix}/.. ABSOLUTE)
    elseif(${dep} STREQUAL folly)
      if (FOLLY_EXTERN)
        get_target_property(folly_lib ${FOLLY_EXTERN} INTERFACE_LOCATION)
        get_filename_component(prefix ${folly_lib} DIRECTORY)
        get_filename_component(prefix ${prefix}/.. ABSOLUTE)
      else()
        set(prefix "internal copy (folly::Function only)")
      endif()
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
if(BUILD_CORE)
  set(core_summary "${BGreen}YES${CR}")
else()
  set(core_summary "${BRed} NO${CR}")
endif()
message(STATUS "  ${BWhite}core${CR}        ${core_summary}")
if(BUILD_TESTING)
  set(tests_summary "${BGreen}YES${CR}    (disable with ${BMagenta}-DBUILD_TESTING=OFF${CR})")
else()
  set(tests_summary "${BRed} NO${CR}    (default, enable with ${BMagenta}-DBUILD_TESTING=ON${CR})")
endif()
message(STATUS "  ${BWhite}tests${CR}       ${tests_summary}")
message(STATUS "  ")
################################################################################
