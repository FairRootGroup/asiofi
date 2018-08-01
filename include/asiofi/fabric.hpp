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
#include <asiofi/info.hpp>
#include <rdma/fabric.h>

namespace asiofi
{
  /**
   * @struct fabric fabric.hpp <asiofi/fabric.hpp>
   * @brief Wraps fid_fabric
   */
  struct fabric
  {
    /// get wrapped C object
    friend auto get_wrapped_obj(const fabric& fabric) -> fid_fabric*
    {
      return fabric.m_fabric;
    }

    /// ctor #1
    explicit fabric(const info& info)
    : m_context({nullptr, nullptr, nullptr, nullptr})
    , m_info(info)
    {
      auto rc = fi_fabric(get_wrapped_obj(info)->fabric_attr, &m_fabric,
                          &m_context);
      if (rc != FI_SUCCESS)
        throw runtime_error("Failed opening ofi fabric, reason: ",
          fi_strerror(rc));
    }

    /// (default) ctor #2
    fabric() : fabric(info()) { }

    /// copy ctor
    explicit fabric(const fabric&) = delete;

    /// move ctor
    explicit fabric(fabric&& rhs)
    : m_info(std::move(rhs.m_info))
    , m_context(std::move(rhs.m_context))
    , m_fabric(std::move(rhs.m_fabric))
    {
      rhs.m_fabric = nullptr;
    }

    /// dtor
    ~fabric() { fi_close(&m_fabric->fid); }

    /// get associated info object
    auto get_info() const -> const info& { return m_info; }

    private:
    const info& m_info;
    fi_context m_context;
    fid_fabric* m_fabric; // TODO use smart pointer
  }; /* struct fabric */
} /* namespace asiofi */

#endif /* ASIOFI_FABRIC_HPP */
