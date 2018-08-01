/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_DETAIL_UTILITY_HPP
#define ASIOFI_DETAIL_UTILITY_HPP

#include <initializer_list>
#include <sstream>
#include <string>

namespace asiofi
{
  namespace detail
  {
    /// concatenates a variable number of args with the << operator
    /// via a stringstream
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
} /* namespace asiofi */

#endif /* ASIOFI_DETAIL_UTILITY_HPP */
