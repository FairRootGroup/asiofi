/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_FABRIC_HPP
#define ASIOFI_FABRIC_HPP

#include <asiofi/errno.hpp>
#include <cstdint>
#include <ostream>
#include <rdma/fabric.h>

namespace asiofi
{

/**
 * @struct hints fabric.hpp <asiofi/fabric.hpp>
 * @brief wraps the fi_info struct
 */
  struct hints
  {
    friend class info;

    /// default query ctor
    hints() : m_info(fi_allocinfo()) { }

    /// copy ctor
    explicit hints(const hints& rh) : m_info(fi_dupinfo(rh.get())) { }

    /// move ctor
    explicit hints(hints&& rh)
    {
      m_info = rh.m_info;
      rh.m_info = nullptr;
    }

    /// dtor
    ~hints() { fi_freeinfo(m_info); }

    friend auto operator<<(std::ostream& os, const hints& hints) -> std::ostream&
    {
      return os << fi_tostr(hints.m_info, FI_TYPE_INFO);
    }

    protected:
    fi_info* m_info;

    auto get() const -> const fi_info* { return m_info; }
  }; /* struct hint */

/**
 * @struct info fabric.hpp <asiofi/fabric.hpp>
 * @brief adds query ctors to asiofi::hint
 */
  struct info : hints
  {
    /// query ctor
    explicit info(const char* node, const char* service,
      uint64_t flags, const hints& hints)
    {
      auto rc = fi_getinfo(FI_VERSION(1, 6), node, service, flags, hints.get(), &m_info);
      if (rc != 0)
        throw runtime_error("Failed querying fi_getinfo, reason: ", fi_strerror(rc));
    }

    /// query ctor #2
    explicit info(uint64_t flags, const hints& hints) : info(nullptr, nullptr, flags, hints) { }

    /// (default) query ctor #3
    info() : info(0, hints()) { }

    /// copy ctor
    explicit info(const info& rh) : hints(rh) { }

    /// move ctor
    explicit info(info&& rh) : hints(rh) { }
  }; /* struct info */

} /* namespace asiofi */

#endif /* ASIOFI_FABRIC_HPP */
