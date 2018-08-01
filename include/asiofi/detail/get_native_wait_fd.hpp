/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_DETAIL_GET_NATIVE_WAIT_FD_HPP
#define ASIOFI_DETAIL_GET_NATIVE_WAIT_FD_HPP

#include <asiofi/errno.hpp>
#include <rdma/fabric.h>

namespace asiofi
{
  namespace detail
  {
    /// Retrieve the native file descriptor for waiting on asynchronous
    /// operations on OFI objects.
    /// @param fid* OFI object asynchronous operations are performed on
    /// @return int wait file descriptor
    auto get_native_wait_fd(fid* obj) -> int
    {
      int fd;

      auto rc = fi_control(obj, FI_GETWAIT, static_cast<void*>(&fd));
      if (rc != FI_SUCCESS)
        throw runtime_error("Failed retrieving native wait fd, reason: ",
          fi_strerror(rc));

      return fd;
    }
  } /* namespace detail */
} /* namespace asiofi */

#endif /* ASIOFI_DETAIL_GET_NATIVE_WAIT_FD_HPP */
