/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_ERRNO_HPP
#define ASIOFI_ERRNO_HPP

#include <asiofi/detail/utility.hpp>
#include <rdma/fi_errno.h>
#include <stdexcept>
#include <utility>

namespace asiofi
{
  /**
   * @struct runtime_error errno.hpp <asiofi/errno.hpp>
   * @brief Custom runtime error with convenient variadic string ctor
   */
  struct runtime_error : ::std::runtime_error
  {
    template<typename... T>
    runtime_error(T&&... t)
      : ::std::runtime_error::runtime_error(detail::to_s(std::forward<T>(t)...))
      , error_code(-1337)
    {}

    template<typename... T>
    runtime_error(int rc, T&&... t)
      : ::std::runtime_error::runtime_error(
          detail::to_s(std::forward<T>(t)..., ", ofi error: ", fi_strerror(rc)))
      , error_code(rc)
    {}

    runtime_error(int rc)
      : ::std::runtime_error::runtime_error(
          detail::to_s("ofi error: ", fi_strerror(rc)))
      , error_code(rc)
    {}

    int error_code;
  };
} /* namespace asiofi */

#endif /* ASIOFI_ERRNO_HPP */
