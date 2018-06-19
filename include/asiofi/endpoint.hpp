/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_ENDPOINT_HPP
#define ASIOFI_ENDPOINT_HPP

#include <asiofi/domain.hpp>
#include <asiofi/errno.hpp>
#include <asiofi/fabric.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <iostream>
#include <rdma/fi_cm.h>
#include <rdma/fi_endpoint.h>
#include <type_traits>

namespace asiofi
{

/**
 * @struct endpoint endpoint.hpp <asiofi/endpoint.hpp>
 * @brief Wraps fid_ep
 */
struct endpoint
{
  /// get wrapped C object
  friend auto get_wrapped_obj(const endpoint& ep) -> fid_ep* { return ep.m_endpoint; }

  /// ctor #1
  explicit endpoint(boost::asio::io_context& io_context, const domain& domain)
  : m_io_context(io_context)
  , m_domain(domain)
  , m_endpoint(create_endpoint(domain, m_context))
  , m_eq(create_event_queue(domain.get_fabric(), m_context))
  , m_eq_fd(io_context, get_native_wait_fd(m_eq))
  {
    // bind event queue to endpoint registering for connection events
    auto rc = fi_ep_bind(m_endpoint, &m_eq->fid, 0);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed binding ofi event queue to ofi endpoint, reason: ", fi_strerror(rc));
  }

  /// (default) ctor
  explicit endpoint() = delete;

  /// copy ctor
  explicit endpoint(const endpoint& rh) = delete;

  /// move ctor
  explicit endpoint(endpoint&& rhs)
  : m_context(std::move(rhs.m_context))
  , m_io_context(rhs.m_io_context)
  , m_domain(std::move(rhs.m_domain))
  , m_endpoint(std::move(rhs.m_endpoint))
  , m_eq(rhs.m_eq)
  , m_eq_fd(std::move(rhs.m_eq_fd))
  {
    rhs.m_endpoint = nullptr;
  }

  /// dtor
  ~endpoint()
  {
    fi_close(&m_eq->fid);
    fi_close(&m_endpoint->fid);
  }

  /// transition endpoint to enabled state
  auto enable() -> void
  {
    auto rc = fi_enable(m_endpoint);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed transitioning ofi endpoint to enabled state, reason: ", fi_strerror(rc));
  }

  template<typename T>
  auto connect(const T& param) -> void
  {
    int rc;
    if (std::is_same<NoParam, T>::value) {
      rc = fi_connect(m_endpoint,
                      get_wrapped_obj(m_domain.get_fabric().get_info())->dest_addr,
                      nullptr,
                      0);
    } else {
      rc = fi_connect(m_endpoint,
                      get_wrapped_obj(m_domain.get_fabric().get_info())->dest_addr,
                      &param,
                      sizeof(T));
    }
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed initiating connection to ", "", " on ofi endpoint, reason: ", fi_strerror(rc));

    auto wait_set = &m_eq->fid;
    rc = fi_trywait(get_wrapped_obj(m_domain.get_fabric()), &wait_set, 1);
    if (rc == FI_SUCCESS) {
      m_eq_fd.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [](const boost::system::error_code& error) {
          if (!error) {
            std::cout << "CM event(s) pending" << std::endl;
          }
        }
      );
    } else {
      throw runtime_error("Not yet implemented");
    }
  }
  struct NoParam { };
  auto connect() -> void { connect<NoParam>(NoParam()); }

  private:
  fi_context m_context;
  boost::asio::io_context& m_io_context;
  const domain& m_domain;
  fid_ep* m_endpoint;
  fid_eq* m_eq;
  boost::asio::posix::stream_descriptor m_eq_fd;

  static auto create_endpoint(const domain& domain, fi_context& context) -> fid_ep* 
  {
    fid_ep* ep;
    auto rc = fi_endpoint(get_wrapped_obj(domain),
                          get_wrapped_obj(domain.get_info()),
                          &ep,
                          &context);
    if (rc != 0)
      throw runtime_error("Failed creating ofi endpoint, reason: ", fi_strerror(rc));

    return ep;
  }

  static auto create_event_queue(const fabric& fabric, fi_context& context) -> fid_eq* 
  {
    fid_eq* eq;
    fi_eq_attr eq_attr = {
      100,         // size_t               size;             [> # entries for EQ <]
      0,           // uint64_t             flags;            [> operation flags <]
      FI_WAIT_FD,  // enum fi_wait_obj     wait_obj;         [> requested wait object <]
      0,           // int                  signaling_vector; [> interrupt affinity <]
      nullptr      // struct fid_wait*     wait_set;         [> optional wait set <]
    };
    auto rc = fi_eq_open(get_wrapped_obj(fabric),
                         &eq_attr,
                         &eq,
                         &context);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed opening ofi event queue, reason: ", fi_strerror(rc));

    return eq;
  }

  static auto get_native_wait_fd(fid_eq* eq) -> int
  {
    int fd;
    auto rc = fi_control(&eq->fid, FI_GETWAIT, static_cast<void*>(&fd));
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed retrieving native wait fd from ofi event queue, reason: ", fi_strerror(rc));

    return fd;
  }

}; /* struct endpoint */

using ep = endpoint;

/**
 * @struct passive_endpoint endpoint.hpp <asiofi/endpoint.hpp>
 * @brief Wraps fid_pep
 */
struct passive_endpoint
{
  /// get wrapped C object
  friend auto get_wrapped_obj(const passive_endpoint& pep) -> fid_pep* { return pep.m_pep; }

  /// ctor #1
  explicit passive_endpoint(boost::asio::io_context& io_context, const fabric& fabric)
  : m_fabric(fabric)
  , m_pep(create_passive_endpoint(fabric, m_context))
  , m_io_context(io_context)
  , m_eq(create_event_queue(fabric, m_context))
  , m_eq_fd(io_context, get_native_wait_fd(m_eq))
  {
    // bind event queue to passive endpoint registering for connection requests
    auto rc = fi_pep_bind(m_pep, &m_eq->fid, FI_CONNREQ);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed binding ofi event queue to ofi passive_ep, reason: ", fi_strerror(rc));
  }

  /// (default) ctor
  explicit passive_endpoint() = delete;

  /// copy ctor
  explicit passive_endpoint(const passive_endpoint& rh) = delete;

  /// move ctor
  explicit passive_endpoint(passive_endpoint&& rhs)
  : m_fabric(std::move(rhs.m_fabric))
  , m_context(std::move(rhs.m_context))
  , m_pep(rhs.m_pep)
  , m_io_context(rhs.m_io_context)
  , m_eq(rhs.m_eq)
  , m_eq_fd(std::move(rhs.m_eq_fd))
  {
    rhs.m_pep = nullptr;
    rhs.m_eq = nullptr;
  }

  /// dtor
  ~passive_endpoint()
  {
    fi_close(&m_eq->fid);
    fi_close(&m_pep->fid);
  }

  /// Listen for connection requests
  auto listen() -> void
  {
    auto rc = fi_listen(m_pep);
    if (rc != 0)
      throw runtime_error("Failed listening on ofi passive_ep, reason: ", fi_strerror(rc));

    auto wait_set = &m_eq->fid;
    rc = fi_trywait(get_wrapped_obj(m_fabric), &wait_set, 1);
    if (rc == FI_SUCCESS) {
      m_eq_fd.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [](const boost::system::error_code& error) {
          if (!error) {
            std::cout << "CM event(s) pending" << std::endl;
          }
        }
      );
    } else {
      throw runtime_error("Not yet implemented");
    }
  }

  template<typename T>
  auto reject(fid_t handle, const T& param) -> void
  {
    auto rc = fi_reject(m_pep, handle, &param, sizeof(T));
    if (rc != 0)
      throw runtime_error("Failed rejecting connection request (", handle, ") on ofi passive_ep, reason: ", fi_strerror(rc));
  }

  auto reject(fid_t handle) -> void
  {
    auto rc = fi_reject(m_pep, handle, nullptr, 0);
    if (rc != 0)
      throw runtime_error("Failed rejecting connection request (", handle, ") on ofi passive_ep, reason: ", fi_strerror(rc));
  }

  private:
  const fabric& m_fabric;
  fi_context m_context;
  fid_pep* m_pep;
  boost::asio::io_context& m_io_context;
  fid_eq* m_eq;
  boost::asio::posix::stream_descriptor m_eq_fd;

  static auto get_native_wait_fd(fid_eq* eq) -> int
  {
    int fd;
    auto rc = fi_control(&eq->fid, FI_GETWAIT, static_cast<void*>(&fd));
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed retrieving native wait fd from ofi event queue, reason: ", fi_strerror(rc));

    return fd;
  }

  static auto create_passive_endpoint(const fabric& fabric, fi_context& context) -> fid_pep* 
  {
    fid_pep* pep;
    auto rc = fi_passive_ep(get_wrapped_obj(fabric),
                            get_wrapped_obj(fabric.get_info()),
                            &pep,
                            &context);
    if (rc != 0)
      throw runtime_error("Failed creating ofi passive_ep, reason: ", fi_strerror(rc));

    return pep;
  }

  static auto create_event_queue(const fabric& fabric, fi_context context) -> fid_eq* 
  {
    fid_eq* eq;
    fi_eq_attr eq_attr = {
      100,         // size_t               size;             [> # entries for EQ <]
      0,           // uint64_t             flags;            [> operation flags <]
      FI_WAIT_FD,  // enum fi_wait_obj     wait_obj;         [> requested wait object <]
      0,           // int                  signaling_vector; [> interrupt affinity <]
      nullptr      // struct fid_wait*     wait_set;         [> optional wait set <]
    };
    auto rc = fi_eq_open(get_wrapped_obj(fabric),
                         &eq_attr,
                         &eq,
                         &context);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed opening ofi event queue, reason: ", fi_strerror(rc));

    return eq;
  }
}; /* struct passive_endpoint */

using pep = passive_endpoint;

} /* namespace asiofi */

#endif /* ASIOFI_ENDPOINT_HPP */
