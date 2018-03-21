/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <asiofi/fabric.hpp>
#include <asiofi/version.hpp>
#include <rdma/fabric.h>
#include <iostream>

int main(int argc, const char* argv[])
{
  std::cout << "asiofi version " << ASIOFI_GIT_VERSION << " from " << ASIOFI_GIT_DATE << std::endl;

  asiofi::info info;
  std::cout << info << std::endl;

  return 0;
}
