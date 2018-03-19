################################################################################
#    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(AsiofiInit)

set_asiofi_cmake_policies()

get_git_version(OUTVAR_PREFIX Asiofi)

project(Asiofi VERSION ${Asiofi_VERSION} LANGUAGES C CXX)
message("${BWhite}${PROJECT_NAME}${CR} ${Asiofi_GIT_VERSION} from ${Asiofi_DATE}")
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Checks for cached boost variable which indicates if Boost is already found
if(Boost_INCLUDE_DIR)
  set(SILENCE_BOOST QUIET)
endif()
find_package(Boost 1.64 ${SILENCE_BOOST} COMPONENTS system)

# generate compile_commands.json file (https://clang.llvm.org/docs/JSONCompilationDatabase.html)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Define CMAKE_INSTALL_*DIR family of variables
include(GNUInstallDirs)

# Define install dirs
set(Asiofi_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
set(Asiofi_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME_LOWER})
set(Asiofi_INSTALL_INCDIR ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME_LOWER})
set(Asiofi_INSTALL_DATADIR ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME_LOWER})

# Configure Version.hpp
configure_file(include/${PROJECT_NAME_LOWER}/version.hpp.in
  ${CMAKE_BINARY_DIR}/include/${PROJECT_NAME_LOWER}/version.hpp
  @ONLY
)

# Define the library
add_library(asiofi INTERFACE)
target_link_libraries(asiofi INTERFACE Boost::boost Boost::system)
target_include_directories(asiofi INTERFACE
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
install(TARGETS asiofi
  EXPORT ${PROJECT_NAME}Targets
  DESTINATION ${Asiofi_INSTALL_LIBDIR}
)
install(
  DIRECTORY
  include/${PROJECT_NAME_LOWER}
  ${CMAKE_BINARY_DIR}/include/${PROJECT_NAME_LOWER}

  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PATTERN *.in EXCLUDE
)

# Add fabtests
add_subdirectory(fabtests)

# Configure/Install CMake package and exported targets
include(CMakePackageConfigHelpers)
set(PACKAGE_INSTALL_DESTINATION
  ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
)
install(EXPORT ${PROJECT_NAME}Targets
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${PACKAGE_INSTALL_DESTINATION}
  EXPORT_LINK_INTERFACE_LIBRARIES
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  COMPATIBILITY AnyNewerVersion
)
configure_package_config_file(
  ${CMAKE_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  INSTALL_DESTINATION ${PACKAGE_INSTALL_DESTINATION}
  PATH_VARS CMAKE_INSTALL_PREFIX
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  DESTINATION ${PACKAGE_INSTALL_DESTINATION}
)
