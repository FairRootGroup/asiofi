/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_INFO_HPP
#define ASIOFI_INFO_HPP

#include <asiofi/errno.hpp>
#include <ostream>
#include <string>
#include <string.h> // strdup
#include <rdma/fabric.h>
#include <utility>

namespace asiofi
{
  /**
   * @struct hints info.hpp <asiofi/info.hpp>
   * @brief wraps the fi_info struct
   */
  struct hints
  {
    /// get wrapped C object
    friend auto get_wrapped_obj(const hints& hints) -> fi_info*
    {
      return hints.m_info;
    }

    /// (default) query ctor
    explicit hints()
    : m_info(fi_allocinfo())
    {
      m_info->caps = FI_MSG;
      m_info->mode = FI_LOCAL_MR;
      m_info->addr_format = FI_SOCKADDR_IN;
      m_info->fabric_attr->prov_name = strdup("sockets");
      m_info->ep_attr->type = FI_EP_MSG;
      m_info->domain_attr->mr_mode = FI_MR_LOCAL
                                   | FI_MR_VIRT_ADDR
                                   | FI_MR_ALLOCATED
                                   | FI_MR_PROV_KEY;
      m_info->domain_attr->threading = FI_THREAD_SAFE;
      m_info->domain_attr->control_progress = FI_PROGRESS_AUTO;
      m_info->domain_attr->data_progress = FI_PROGRESS_AUTO;
      m_info->domain_attr->resource_mgmt = FI_RM_ENABLED;
      m_info->tx_attr->op_flags = FI_COMPLETION;
      m_info->rx_attr->op_flags = FI_COMPLETION;
    }

    /// copy ctor
    hints(const hints& rhs) : m_info(fi_dupinfo(get_wrapped_obj(rhs))) { }

    /// move ctor
    hints(hints&& rhs)
    : m_info(std::move(rhs.m_info))
    {
      rhs.m_info = nullptr;
    }

    /// dtor
    ~hints() { if (m_info) fi_freeinfo(m_info); }

    friend auto operator<<(std::ostream& os, const hints& hints)
    -> std::ostream&
    {
      return os << fi_tostr(hints.m_info, FI_TYPE_INFO);
    }

    auto set_provider(const std::string& provider) -> void
    {
      // TODO fix memory leak
      m_info->fabric_attr->prov_name = strdup(provider.c_str());
    }

    auto set_domain(const std::string& domain) -> void
    {
      // TODO fix memory leak
      m_info->domain_attr->name = strdup(domain.c_str());
    }

    auto set_destination(const std::string& address, const std::string& port) -> void
    {
      // TODO check addr format field
      sockaddr_in* sa(new sockaddr_in());
      (void)inet_pton(AF_INET, address.c_str(), &(sa->sin_addr));
      sa->sin_port = htons(std::stoi(port));
      sa->sin_family = AF_INET;

      m_info->dest_addr = sa;
      m_info->dest_addrlen = sizeof(sockaddr_in);
    }

    protected:
    fi_info* m_info; // TODO use smart pointer

    /// adoption ctor
    explicit hints(fi_info* adopted_info) : m_info(adopted_info) { }
  }; /* struct hints */

  /**
   * @struct info info.hpp <asiofi/info.hpp>
   * @brief adds query ctors to asiofi::hint
   */
  struct info : hints
  {
    /// query ctor
    explicit info(const char* node, const char* service,
      uint64_t flags, const hints& hints)
    {
      auto rc = fi_getinfo(FI_VERSION(1, 6), node, service, flags,
                           get_wrapped_obj(hints), &m_info);
      if (rc == -61) {
        throw runtime_error("Failed querying fi_getinfo, reason: ",
          "No supported fabric/domain found (", rc, ")");
      } else if (rc != 0)
        throw runtime_error("Failed querying fi_getinfo, reason: ",
          fi_strerror(rc));
    }

    /// query ctor #2
    explicit info(uint64_t flags, const hints& hints)
    : info(nullptr, nullptr, flags, hints)
    {
    }

    /// query ctor #3
    explicit info(const hints& hints) : info(0, hints) { }

    /// (default) query ctor #4
    info() : info(hints()) { }

    /// adoption ctor
    explicit info(fi_info* adopted_info) : hints(adopted_info) { }

    /// copy ctor
    info(const info& rhs) : hints(rhs) { }

    /// move ctor
    info(info&& rhs) : hints(rhs) { }
  }; /* struct info */
} /* namespace asiofi */

#endif /* ASIOFI_INFO_HPP */
