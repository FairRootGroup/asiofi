/********************************************************************************
 * Copyright (C) 2018-2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_DOMAIN_HPP
#define ASIOFI_DOMAIN_HPP

#include <asiofi/errno.hpp>
#include <asiofi/fabric.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <rdma/fi_domain.h>

namespace asiofi
{
  /**
   * @struct domain domain.hpp <asiofi/domain.hpp>
   * @brief Wraps fid_domain
   */
  struct domain
  {
    /// get wrapped C object
    friend auto get_wrapped_obj(const domain& domain) -> fid_domain*
    {
      return domain.m_domain;
    }

    /// ctor #1
    explicit domain(const fabric& fabric)
    : m_fabric(fabric)
    {
      auto rc = fi_domain(get_wrapped_obj(fabric),
                          get_wrapped_obj(fabric.get_info()),
                          &m_domain,
                          &m_context);
      if (rc != 0)
        throw runtime_error("Failed opening ofi domain, reason: ",
          fi_strerror(rc));
    }

    /// (default) ctor
    explicit domain() = delete;

    /// copy ctor
    domain(const domain&) = delete;

    /// move ctor
    domain(domain&& rhs)
    : m_fabric(std::move(rhs.m_fabric))
    , m_context(std::move(rhs.m_context))
    , m_domain(std::move(rhs.m_domain))
    {
      rhs.m_domain = nullptr;
    }

    /// dtor
    ~domain() { fi_close(&m_domain->fid); }

    /// get associated fabric object
    auto get_fabric() const -> const fabric& { return m_fabric; }

    /// get associated info object
    auto get_info() const -> const info& { return m_fabric.get_info(); }

    private:
    const fabric& m_fabric;
    fi_context m_context;
    fid_domain* m_domain; // TODO use smart pointer
  }; /* struct domain */

} /* namespace asiofi */

#endif /* ASIOFI_DOMAIN_HPP */
