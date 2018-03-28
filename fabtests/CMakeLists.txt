################################################################################
#    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    #
#                                                                              #
#              This software is distributed under the terms of the             #
#              GNU Lesser General Public Licence (LGPL) version 3,             #
#                  copied verbatim in the file "LICENSE"                       #
################################################################################

add_executable(afi_msg_bw benchmarks/msg_bw.cpp)
target_link_libraries(afi_msg_bw PUBLIC asiofi)

add_executable(malloc_bw benchmarks/malloc_bw.cpp)
target_link_libraries(malloc_bw PUBLIC benchmark::benchmark)

add_executable(shmalloc_bw benchmarks/shmalloc_bw.cpp)
target_link_libraries(shmalloc_bw PUBLIC benchmark::benchmark Boost::boost)

install(
  TARGETS
  afi_msg_bw
  malloc_bw
  shmalloc_bw

  EXPORT ${PROJECT_NAME}Targets
  DESTINATION ${Asiofi_INSTALL_BINDIR}
)
