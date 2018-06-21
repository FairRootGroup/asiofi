/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_FABRIC_HPP
#define ASIOFI_FABRIC_HPP

#include <algorithm> // std::move
#include <asiofi/errno.hpp>
#include <cstdint>
#include <ostream>
#include <string>
#include <string.h> // strdup
#include <rdma/fabric.h>

namespace asiofi
{

//
  // sockaddr_in* sa = static_cast<sockaddr_in*>(malloc(sizeof(sockaddr_in)));
  // addr.Port = 0;
  // auto sa2 = ConvertAddress(addr);
  // memcpy(sa, &sa2, sizeof(sockaddr_in));
//
  //Prepare fi_getinfo query
  // unique_ptr<fi_info, void(*)(fi_info*)> hints(fi_allocinfo(), fi_freeinfo);
  // hints->caps = FI_MSG;
  //ofi_hints->mode = FI_CONTEXT;
  // hints->addr_format = FI_SOCKADDR_IN;
  // if (addr.Protocol == "tcp") {
      // hints->fabric_attr->prov_name = strdup("sockets");
  // } else if (addr.Protocol == "verbs") {
      // hints->fabric_attr->prov_name = strdup("verbs;ofi_rxm");
  // }
  // hints->ep_attr->type = FI_EP_RDM;
  //ofi_hints->domain_attr->mr_mode = FI_MR_BASIC | FI_MR_SCALABLE;
  // hints->domain_attr->threading = FI_THREAD_SAFE;
  // hints->domain_attr->control_progress = FI_PROGRESS_AUTO;
  // hints->domain_attr->data_progress = FI_PROGRESS_AUTO;
  // hints->tx_attr->op_flags = FI_COMPLETION;
  // hints->rx_attr->op_flags = FI_COMPLETION;
  // if (vm.count("host")) {
      // ofi_hints->src_addr = sa;
      // ofi_hints->src_addrlen = sizeof(sockaddr_in);
      // ofi_hints->dest_addr = nullptr;
      // ofi_hints->dest_addrlen = 0;
  // } else {
      // ofi_hints->src_addr = nullptr;
      // ofi_hints->src_addrlen = 0;
      // ofi_hints->dest_addr = sa;
      // ofi_hints->dest_addrlen = sizeof(sockaddr_in);
  // }

/**
 * @struct hints fabric.hpp <asiofi/fabric.hpp>
 * @brief wraps the fi_info struct
 */
struct hints
{
  /// get wrapped C object
  friend auto get_wrapped_obj(const hints& hints) -> fi_info* { return hints.m_info; }

  /// (default) query ctor
  hints() : m_info(fi_allocinfo())
  {
    m_info->caps = FI_MSG;
    m_info->mode = FI_CONTEXT;
    m_info->addr_format = FI_SOCKADDR_IN;
    m_info->fabric_attr->prov_name = strdup("sockets");
    m_info->ep_attr->type = FI_EP_MSG;
    // m_info->domain_attr->mr_mode = FI_MR_BASIC | FI_MR_SCALABLE;
    m_info->domain_attr->threading = FI_THREAD_SAFE;
    m_info->domain_attr->control_progress = FI_PROGRESS_AUTO;
    m_info->domain_attr->data_progress = FI_PROGRESS_AUTO;
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
  ~hints() { fi_freeinfo(m_info); }

  friend auto operator<<(std::ostream& os, const hints& hints) -> std::ostream&
  {
    return os << fi_tostr(hints.m_info, FI_TYPE_INFO);
  }

  auto set_provider(const std::string& provider) -> void
  {
    // TODO fix memory leak
    m_info->fabric_attr->prov_name = strdup(provider.c_str());
  }

  protected:
  fi_info* m_info;

  /// adoption ctor
  explicit hints(fi_info* adopted_info) : m_info(adopted_info) { }
}; /* struct hints */

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
    auto rc = fi_getinfo(FI_VERSION(1, 6), node, service, flags, get_wrapped_obj(hints), &m_info);
    if (rc == -61) {
      throw runtime_error("Failed querying fi_getinfo, reason: Requested configuration cannot be satisfied (", rc, ")");
    } else if (rc != 0)
      throw runtime_error("Failed querying fi_getinfo, reason: ", fi_strerror(rc));
  }

  /// query ctor #2
  explicit info(uint64_t flags, const hints& hints) : info(nullptr, nullptr, flags, hints) { }

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

/**
 * @struct fabric fabric.hpp <asiofi/fabric.hpp>
 * @brief Wraps fid_fabric
 */
struct fabric
{
  /// get wrapped C object
  friend auto get_wrapped_obj(const fabric& fabric) -> fid_fabric* { return fabric.m_fabric; }

  /// ctor #1
  explicit fabric(const info& info)
  : m_info(info)
  {
    auto rc = fi_fabric(get_wrapped_obj(info)->fabric_attr, &m_fabric, &m_context);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed opening ofi fabric, reason: ", fi_strerror(rc));
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
  fid_fabric* m_fabric;
}; /* struct fabric */

} /* namespace asiofi */

#endif /* ASIOFI_FABRIC_HPP */
