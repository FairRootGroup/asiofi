/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_ERRNO_HPP
#define ASIOFI_ERRNO_HPP

#include <initializer_list>
#include <rdma/fi_errno.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace asiofi
{

  namespace detail
  {
    /// @brief concatenates a variable number of args with the << operator via a stringstream
    /// @param t objects to be concatenated
    /// @return concatenated string
    template<typename ...T>
    auto to_s(T&&... t) -> std::string
    {
      std::stringstream ss;
      (void)std::initializer_list<int>{(ss << t, 0)...};
      return ss.str();
    }
  } /* namespace detail */

  struct runtime_error : ::std::runtime_error
  {
    template<typename ...T>
    runtime_error(T&&... t)
    : ::std::runtime_error::runtime_error(detail::to_s(std::forward<T>(t)...)) { }
  };
} /* namespace asiofi */

#endif /* ASIOFI_ERRNO_HPP */
