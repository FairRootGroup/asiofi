/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_DOMAIN_HPP
#define ASIOFI_DOMAIN_HPP

#include <asiofi/errno.hpp>
#include <asiofi/fabric.hpp>
#include <boost/asio/buffer.hpp>
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
  friend auto get_wrapped_obj(const domain& domain) -> fid_domain* { return domain.m_domain; }

  /// ctor #1
  explicit domain(const fabric& fabric)
  : m_fabric(fabric)
  {
    auto rc = fi_domain(get_wrapped_obj(fabric),
                        get_wrapped_obj(fabric.get_info()),
                        &m_domain,
                        &m_context);
    if (rc != 0)
      throw runtime_error("Failed opening ofi domain, reason: ", fi_strerror(rc));
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
  fid_domain* m_domain;
}; /* struct domain */

/**
 * @struct address_vector domain.hpp <asiofi/domain.hpp>
 * @brief Wraps fid_av
 */
struct address_vector
{
  /// ctor #1
  explicit address_vector(const domain& domain)
  {
    fi_av_attr av_attr = {
      get_wrapped_obj(domain.get_info())->domain_attr->av_type, // enum fi_av_type  type;        [> type of AV <]
      0,                                             // int              rx_ctx_bits; [> address bits to identify rx ctx <]
      1000,                                          // size_t           count;       [> # entries for AV <]
      0,                                             // size_t           ep_per_node; [> # endpoints per fabric address <]
      nullptr,                                       // const char       *name;       [> system name of AV <]
      nullptr,                                       // void             *map_addr;   [> base mmap address <]
      0                                              // uint64_t         flags;       [> operation flags <]
    };
    auto rc = fi_av_open(get_wrapped_obj(domain), &av_attr, &m_av, &m_context);
    if (rc != 0)
      throw runtime_error("Failed opening ofi address vector, reason: ", fi_strerror(rc));
  }

  /// (default) ctor
  explicit address_vector() = delete;

  /// copy ctor
  address_vector(const address_vector&) = delete;

  /// move ctor
  address_vector(address_vector&& rhs)
  : m_context(std::move(rhs.m_context))
  , m_av(std::move(rhs.m_av))
  {
    rhs.m_av = nullptr;
  }

  /// dtor
  ~address_vector() { fi_close(&m_av->fid); }

  protected:
  fi_context m_context;
  fid_av* m_av;
}; /* struct address_vector */

using av = address_vector;


/**
 * @struct memory_region domain.hpp <include/asiofi/domain.hpp>
 * @brief A memory buffer registered for ofi access
 */
struct memory_region
{
  enum class access : uint64_t {
    send = FI_SEND,
    recv = FI_RECV,
    read = FI_READ,
    write = FI_WRITE,
    remote_read = FI_REMOTE_READ,
    remote_write = FI_REMOTE_WRITE
  };

  explicit memory_region(const domain& domain, boost::asio::mutable_buffer buffer, access access)
  : m_memory_region(create_memory_region(domain, buffer, access, m_context))
  {
  }

  memory_region(const memory_region&) = delete;

  memory_region(memory_region&& rhs)
  : m_memory_region(std::move(rhs.m_memory_region))
  {
    m_memory_region = nullptr;
  }

  ~memory_region()
  {
    if (m_memory_region)
      fi_close(&m_memory_region->fid);
  }

  private:
  fi_context m_context;
  fid_mr* m_memory_region;

  static auto create_memory_region(const domain& domain, boost::asio::mutable_buffer buffer, access access, fi_context& context) -> fid_mr*
  {
    fid_mr* mr;
    auto rc = fi_mr_reg(get_wrapped_obj(domain), buffer.data(), buffer.size(), static_cast<uint64_t>(access), 0, 0, 0, &mr, &context);
    if (rc != 0)
      throw runtime_error("Failed registering ofi memory region, reason: ", fi_strerror(rc));

    return mr;
  }
}; /* struct memory_region */

using mr = memory_region;

} /* namespace asiofi */

#endif /* ASIOFI_DOMAIN_HPP */
