################################################################################
#    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

### PUBLIC

# Defines some variables with console color escape sequences
if(NOT WIN32 AND NOT DISABLE_COLOR)
  string(ASCII 27 Esc)
  set(CR       "${Esc}[m")
  set(CB       "${Esc}[1m")
  set(Red      "${Esc}[31m")
  set(Green    "${Esc}[32m")
  set(Yellow   "${Esc}[33m")
  set(Blue     "${Esc}[34m")
  set(Magenta  "${Esc}[35m")
  set(Cyan     "${Esc}[36m")
  set(White    "${Esc}[37m")
  set(BRed     "${Esc}[1;31m")
  set(BGreen   "${Esc}[1;32m")
  set(BYellow  "${Esc}[1;33m")
  set(BBlue    "${Esc}[1;34m")
  set(BMagenta "${Esc}[1;35m")
  set(BCyan    "${Esc}[1;36m")
  set(BWhite   "${Esc}[1;37m")
endif()

find_package(Git)
# get_git_version([DEFAULT_VERSION version] [DEFAULT_DATE date] [OUTVAR_PREFIX prefix])
#
# Sets variables #prefix#_VERSION, #prefix#_GIT_VERSION, #prefix#_DATE, #prefix#_GIT_DATE
function(get_git_version)
  cmake_parse_arguments(ARGS "" "DEFAULT_VERSION;DEFAULT_DATE;OUTVAR_PREFIX" "" ${ARGN})

  if(NOT ARGS_OUTVAR_PREFIX)
    set(ARGS_OUTVAR_PREFIX ASIOFI)
  endif()

  if(GIT_FOUND AND EXISTS ${CMAKE_SOURCE_DIR}/.git)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --match "v*"
      OUTPUT_VARIABLE ${ARGS_OUTVAR_PREFIX}_GIT_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    if(${ARGS_OUTVAR_PREFIX}_GIT_VERSION)
      # cut first two characters "v-"
      string(SUBSTRING ${${ARGS_OUTVAR_PREFIX}_GIT_VERSION} 1 -1 ${ARGS_OUTVAR_PREFIX}_GIT_VERSION)
    endif()
    execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --format=%cd
      OUTPUT_VARIABLE ${ARGS_OUTVAR_PREFIX}_GIT_DATE
      OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  endif()

  if(NOT ${ARGS_OUTVAR_PREFIX}_GIT_VERSION)
    if(ARGS_DEFAULT_VERSION)
      set(${ARGS_OUTVAR_PREFIX}_GIT_VERSION ${ARGS_DEFAULT_VERSION})
    else()
      set(${ARGS_OUTVAR_PREFIX}_GIT_VERSION 0.0.0.0)
    endif()
  endif()
  
  if(NOT ${ARGS_OUTVAR_PREFIX}_GIT_DATE)
    if(ARGS_DEFAULT_DATE)
      set(${ARGS_OUTVAR_PREFIX}_GIT_DATE ${ARGS_DEFAULT_DATE})
    else()
      set(${ARGS_OUTVAR_PREFIX}_GIT_DATE "Thu Jan 1 00:00:00 1970 +0000")
    endif()
  endif()

  string(REGEX MATCH "^([^-]*)" blubb ${${ARGS_OUTVAR_PREFIX}_GIT_VERSION})

  # return values
  set(${ARGS_OUTVAR_PREFIX}_VERSION ${CMAKE_MATCH_0} PARENT_SCOPE)
  set(${ARGS_OUTVAR_PREFIX}_DATE ${${ARGS_OUTVAR_PREFIX}_GIT_DATE} PARENT_SCOPE)
  set(${ARGS_OUTVAR_PREFIX}_GIT_VERSION ${${ARGS_OUTVAR_PREFIX}_GIT_VERSION} PARENT_SCOPE)
  set(${ARGS_OUTVAR_PREFIX}_GIT_DATE ${${ARGS_OUTVAR_PREFIX}_GIT_DATE} PARENT_SCOPE)
endfunction()


# Set defaults
macro(set_asiofi_defaults)
  string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

  # Set a default build type
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
  endif()

  # Handle C++ standard level
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
  elseif(${CMAKE_CXX_STANDARD} LESS 14)
    message(FATAL_ERROR "A minimum CMAKE_CXX_STANDARD of 14 is required.")
  endif()
  if(NOT CMAKE_CXX_EXTENSIONS)
    set(CMAKE_CXX_EXTENSIONS OFF)
  endif()

  # Generate compile_commands.json file (https://clang.llvm.org/docs/JSONCompilationDatabase.html)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

  # Define CMAKE_INSTALL_*DIR family of variables
  include(GNUInstallDirs)

  # Define install dirs
  set(asiofi_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
  set(asiofi_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME_LOWER})
  set(asiofi_INSTALL_INCDIR ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME_LOWER})
  set(asiofi_INSTALL_DATADIR ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME_LOWER})
  set(asiofi_INSTALL_CMAKEMODDIR ${asiofi_INSTALL_DATADIR}/cmake)

  # Define export set, only one for now
  set(asiofi_EXPORT_SET ${PROJECT_NAME}Targets)

  # https://cmake.org/Wiki/CMake_RPATH_handling
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
  list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_LIBDIR}" isSystemDir)
  if("${isSystemDir}" STREQUAL "-1")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} "-Wl,--enable-new-dtags")
      set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "-Wl,--enable-new-dtags")
      set(CMAKE_INSTALL_RPATH "$ORIGIN/../${PROJECT_INSTALL_LIBDIR}")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
      set(CMAKE_INSTALL_RPATH "@loader_path/../${PROJECT_INSTALL_LIBDIR}")
    else()
      set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${PROJECT_INSTALL_LIBDIR}")
    endif()
  endif()

  if(CMAKE_GENERATOR STREQUAL "Ninja" AND
     ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9) OR
      (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.5)))
    # Force colored warnings in Ninja's output, if the compiler has -fdiagnostics-color support.
    # Rationale in https://github.com/ninja-build/ninja/issues/814
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
  endif()
endmacro()

function(join VALUES GLUE OUTPUT)
  string(REGEX REPLACE "([^\\]|^);" "\\1${GLUE}" _TMP_STR "${VALUES}")
  string(REGEX REPLACE "[\\](.)" "\\1" _TMP_STR "${_TMP_STR}") #fixes escaping
  set(${OUTPUT} "${_TMP_STR}" PARENT_SCOPE)
endfunction()

function(pad str width char out)
  cmake_parse_arguments(ARGS "" "COLOR" "" ${ARGN})
  string(LENGTH ${str} length)
  if(ARGS_COLOR)
    math(EXPR padding "${width}-(${length}-10*${ARGS_COLOR})")
  else()
    math(EXPR padding "${width}-${length}")
  endif()
  if(padding GREATER 0)
    foreach(i RANGE ${padding})
      set(str "${str}${char}")
    endforeach()
  endif()
  set(${out} ${str} PARENT_SCOPE)
endfunction()

function(generate_package_dependencies)
  join("${PROJECT_INTERFACE_PACKAGE_DEPENDENCIES}" " " DEPS)
  set(PACKAGE_DEPENDENCIES "\
####### Expanded from @PACKAGE_DEPENDENCIES@ by configure_package_config_file() #######

set(${PROJECT_NAME}_PACKAGE_DEPENDENCIES ${DEPS})

")
  foreach(dep IN LISTS PROJECT_INTERFACE_PACKAGE_DEPENDENCIES)
    join("${PROJECT_INTERFACE_${dep}_COMPONENTS}" " " COMPS)
    if(COMPS)
      string(CONCAT PACKAGE_DEPENDENCIES ${PACKAGE_DEPENDENCIES} "\
set(${PROJECT_NAME}_${dep}_COMPONENTS ${COMPS})
")
    endif()
    if(PROJECT_INTERFACE_${dep}_VERSION)
      string(CONCAT PACKAGE_DEPENDENCIES ${PACKAGE_DEPENDENCIES} "\
set(${PROJECT_NAME}_${dep}_VERSION ${PROJECT_INTERFACE_${dep}_VERSION})
")
    endif()
  endforeach()
  string(CONCAT PACKAGE_DEPENDENCIES ${PACKAGE_DEPENDENCIES} "\

#######################################################################################
")
set(PACKAGE_DEPENDENCIES ${PACKAGE_DEPENDENCIES} PARENT_SCOPE)
endfunction()

function(generate_package_components)
  join("${PROJECT_PACKAGE_COMPONENTS}" " " COMPS)
  set(PACKAGE_COMPONENTS "\
####### Expanded from @PACKAGE_COMPONENTS@ by configure_package_config_file() #########

set(${PROJECT_NAME}_PACKAGE_COMPONENTS ${COMPS})

")
  foreach(comp IN LISTS PROJECT_PACKAGE_COMPONENTS)
    string(CONCAT PACKAGE_COMPONENTS ${PACKAGE_COMPONENTS} "\
set(${PROJECT_NAME}_${comp}_FOUND TRUE)
")
  endforeach()
  string(CONCAT PACKAGE_COMPONENTS ${PACKAGE_COMPONENTS} "\

check_required_components(${PROJECT_NAME})
")
set(PACKAGE_COMPONENTS ${PACKAGE_COMPONENTS} PARENT_SCOPE)
endfunction()

# Configure/Install CMake package
macro(install_asiofi_cmake_package)
  # Install cmake modules
  install( FILES cmake/FindOFI.cmake
    DESTINATION ${asiofi_INSTALL_CMAKEMODDIR}
  )

  include(CMakePackageConfigHelpers)
  set(PACKAGE_INSTALL_DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
  )
  install(EXPORT ${asiofi_EXPORT_SET}
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${PACKAGE_INSTALL_DESTINATION}
    EXPORT_LINK_INTERFACE_LIBRARIES
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    COMPATIBILITY AnyNewerVersion
  )
  generate_package_dependencies() # fills ${PACKAGE_DEPENDENCIES}
  generate_package_components() # fills ${PACKAGE_COMPONENTS}
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
endmacro()

macro(find_package2 qualifier pkgname)
  cmake_parse_arguments(ARGS "" "VERSION" "COMPONENTS" ${ARGN})

  string(TOUPPER ${pkgname} pkgname_upper)
  set(old_CPP ${CMAKE_PREFIX_PATH})
  set(CMAKE_PREFIX_PATH ${${pkgname_upper}_ROOT} $ENV{${pkgname_upper}_ROOT} ${CMAKE_PREFIX_PATH})
  if(ARGS_COMPONENTS)
    find_package(${pkgname} ${ARGS_VERSION} QUIET COMPONENTS ${ARGS_COMPONENTS} ${ARGS_UNPARSED_ARGUMENTS})
  else()
    find_package(${pkgname} ${ARGS_VERSION} QUIET ${ARGS_UNPARSED_ARGUMENTS})
  endif()
  set(CMAKE_PREFIX_PATH ${old_CPP})
  unset(old_CPP)

  if(${pkgname}_FOUND)
    if(${qualifier} STREQUAL PRIVATE)
      set(PROJECT_${pkgname}_VERSION ${ARGS_VERSION})
      set(PROJECT_${pkgname}_COMPONENTS ${ARGS_COMPONENTS})
      set(PROJECT_PACKAGE_DEPENDENCIES ${PROJECT_PACKAGE_DEPENDENCIES} ${pkgname})
    elseif(${qualifier} STREQUAL PUBLIC)
      set(PROJECT_${pkgname}_VERSION ${ARGS_VERSION})
      set(PROJECT_${pkgname}_COMPONENTS ${ARGS_COMPONENTS})
      set(PROJECT_PACKAGE_DEPENDENCIES ${PROJECT_PACKAGE_DEPENDENCIES} ${pkgname})
      set(PROJECT_INTERFACE_${pkgname}_VERSION ${ARGS_VERSION})
      set(PROJECT_INTERFACE_${pkgname}_COMPONENTS ${ARGS_COMPONENTS})
      set(PROJECT_INTERFACE_PACKAGE_DEPENDENCIES ${PROJECT_INTERFACE_PACKAGE_DEPENDENCIES} ${pkgname})
    elseif(${qualifier} STREQUAL INTERFACE)
      set(PROJECT_INTERFACE_${pkgname}_VERSION ${ARGS_VERSION})
      set(PROJECT_INTERFACE_${pkgname}_COMPONENTS ${ARGS_COMPONENTS})
      set(PROJECT_INTERFACE_PACKAGE_DEPENDENCIES ${PROJECT_INTERFACE_PACKAGE_DEPENDENCIES} ${pkgname})
    endif()
  endif()
endmacro()
