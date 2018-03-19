################################################################################
#    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

add_executable(afi_msg_bw benchmarks/msg_bw.cpp)
target_link_libraries(afi_msg_bw PUBLIC asiofi)

install(
  TARGETS
  afi_msg_bw

  EXPORT ${PROJECT_NAME}Targets
  DESTINATION ${Asiofi_INSTALL_BINDIR}
)
