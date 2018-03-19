################################################################################
#    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

cmake_minimum_required(VERSION 3.9.4 FATAL_ERROR)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(AsiofiInit)

set_asiofi_cmake_policies()

get_git_version(OUTVAR_PREFIX ASIOFI)

project(asiofi VERSION ${ASIOFI_VERSION} LANGUAGES C CXX)
message("${BWhite}${PROJECT_NAME}${CR} ${PROJECT_VERSION} (${ASIOFI_GIT_VERSION}) from ${ASIOFI_DATE}")
