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
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <functional>
#include <iostream>
#include <rdma/fi_cm.h>
#include <rdma/fi_endpoint.h>
#include <utility>

namespace asiofi
{

/**
 * @struct endpoint endpoint.hpp <asiofi/endpoint.hpp>
 * @brief Wraps fid_ep
 */
struct endpoint
{
  /// get wrapped C object
  friend auto get_wrapped_obj(const endpoint& ep) -> fid_ep* { return ep.m_endpoint.get(); }

  /// ctor #1
  explicit endpoint(boost::asio::io_context& io_context, const domain& domain, const info& info)
  : m_io_context(io_context)
  , m_domain(domain)
  , m_eq(m_io_context, domain.get_fabric())
  , m_rx_strand(m_io_context)
  , m_tx_strand(m_io_context)
  , m_rx_cq(m_rx_strand, cq::direction::rx, domain)
  , m_tx_cq(m_tx_strand, cq::direction::tx, domain)
  , m_endpoint(create_endpoint(domain, info, m_context))
  {
    bind(m_eq);
    bind(m_rx_cq, endpoint::cq_flag::recv);
    bind(m_tx_cq, endpoint::cq_flag::transmit);
  }

  /// ctor #2
  explicit endpoint(boost::asio::io_context& io_context, const domain& domain)
  : endpoint(io_context, domain, domain.get_info())
  {
  }

  endpoint() = delete;

  endpoint(const endpoint& rh) = delete;

  endpoint(endpoint&& rhs) = default;

  auto bind(const event_queue& eq) -> void
  {
    auto rc = fi_ep_bind(m_endpoint.get(), &get_wrapped_obj(eq)->fid, 0);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed binding ofi event queue to ofi endpoint, reason: ", fi_strerror(rc));
  }

  enum class cq_flag : uint64_t {
    transmit = FI_TRANSMIT,
    recv = FI_RECV,
    selective_completion = FI_SELECTIVE_COMPLETION
  };

  auto bind(const completion_queue& cq, endpoint::cq_flag flag) -> void
  {
    auto rc = fi_ep_bind(m_endpoint.get(), &get_wrapped_obj(cq)->fid, static_cast<uint64_t>(flag));
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed binding ofi completion queue to ofi endpoint, reason: ", fi_strerror(rc));
  }

  /// transition endpoint to enabled state
  auto enable() -> void
  {
    auto rc = fi_enable(m_endpoint.get());
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed transitioning ofi endpoint to enabled state, reason: ", fi_strerror(rc));
  }

  template<typename CompletionHandler>
  auto connect(CompletionHandler&& handler) -> void {
    auto rc = fi_connect(m_endpoint.get(),
                         get_wrapped_obj(m_domain.get_fabric().get_info())->dest_addr,
                         nullptr,
                         0);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed initiating connection to ", "", " on ofi endpoint, reason: ", fi_strerror(rc));

    m_eq.read([&](eq::event event, fid_t handle, info&& info){
      if (event == eq::event::connected) {
        handler();
      } else {
        throw runtime_error("Unexpected event read from ofi event queue, expected FI_CONNECTED, got: ", static_cast<uint32_t>(event));
      }
    });
  }

  template<typename CompletionHandler>
  auto accept(CompletionHandler&& handler) -> void
  {
    auto rc = fi_accept(m_endpoint.get(), nullptr, 0);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed accepting connection, reason: ", fi_strerror(rc));

    m_eq.read([&](eq::event event, fid_t handle, info&& info){
      if (event == eq::event::connected) {
        handler();
      } else {
        throw runtime_error("Unexpected event read from ofi event queue, expected FI_CONNECTED, got: ", static_cast<uint32_t>(event));
      }
    });
  }

  template<typename CompletionHandler>
  auto send(boost::asio::mutable_buffer buffer,
            void* mr_desc,
            CompletionHandler&& handler) -> void
  {
    fi_addr_t dummy_addr;
    auto ctx = std::unique_ptr<fi_context>(
      new fi_context{nullptr, nullptr, nullptr, nullptr});

    // std::cout << "fi_send: buf=" << buffer.data()
    //                  << ", len=" << buffer.size()
    //                  << ", desc=" << mr_desc
    //                  << ", ctx=" << ctx.get() << std::endl;
    auto rc = fi_send(m_endpoint.get(), buffer.data(), buffer.size(),
      mr_desc, dummy_addr, ctx.get());
    if (rc != FI_SUCCESS) {
      throw runtime_error("Failed posting a TX buffer on ofi endpoint, reason: ",
        fi_strerror(rc));
    }

    m_tx_cq.read(
      boost::asio::bind_executor(m_tx_strand,
        std::bind(std::forward<CompletionHandler>(handler), buffer)),
      std::move(ctx));
  }

  template<typename CompletionHandler>
  auto send(boost::asio::mutable_buffer buffer,
            CompletionHandler&& handler) -> void
  {
    send(std::move(buffer), nullptr, std::forward<CompletionHandler>(handler));
  }

  template<typename CompletionHandler>
  auto recv(boost::asio::mutable_buffer buffer,
            void* mr_desc,
            CompletionHandler&& handler) -> void
  {
    fi_addr_t dummy_addr;
    auto ctx = std::unique_ptr<fi_context>(
      new fi_context{nullptr, nullptr, nullptr, nullptr});
    
    // std::cout << "fi_recv: buf=" << buffer.data()
    //                  << ", len=" << buffer.size()
    //                  << ", desc=" << mr_desc
    //                  << ", ctx=" << ctx.get() << std::endl;
    auto rc = fi_recv(m_endpoint.get(), buffer.data(), buffer.size(),
      mr_desc, dummy_addr, ctx.get());
    if (rc != FI_SUCCESS) {
      throw runtime_error("Failed posting a RX buffer on ofi endpoint, reason: ",
        fi_strerror(rc));
    }

    m_rx_cq.read(
      boost::asio::bind_executor(m_rx_strand,
        std::bind(std::forward<CompletionHandler>(handler), buffer)),
      std::move(ctx));
  }

  template<typename CompletionHandler>
  auto recv(boost::asio::mutable_buffer buffer,
            CompletionHandler&& handler) -> void
  {
    recv(std::move(buffer), nullptr, std::forward<CompletionHandler>(handler));
  }

  auto shutdown() -> void
  {
    auto rc = fi_shutdown(m_endpoint.get(), 0);
    if (rc != FI_SUCCESS) {
      throw runtime_error("Failed shutting down ofi endpoint, reason: ",
        fi_strerror(rc));
    }
  }

  private:
  using fid_ep_deleter = std::function<void(fid_ep*)>;

  fi_context m_context;
  boost::asio::io_context& m_io_context;
  const domain& m_domain;
  event_queue m_eq;
  boost::asio::io_context::strand m_rx_strand, m_tx_strand;
  completion_queue m_rx_cq, m_tx_cq;
  std::unique_ptr<fid_ep, fid_ep_deleter> m_endpoint;

  static auto create_endpoint(const domain& domain,
                              const info& info,
                              fi_context& context)
  -> std::unique_ptr<fid_ep, fid_ep_deleter>
  {
    fid_ep* ep;
    auto rc = fi_endpoint(get_wrapped_obj(domain),
                          get_wrapped_obj(info),
                          &ep,
                          &context);
    if (rc != 0) {
      throw runtime_error("Failed creating ofi endpoint, reason: ",
        fi_strerror(rc));
    }

    return {ep, [](fid_ep* ep){ fi_close(&ep->fid); }};
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
  friend auto get_wrapped_obj(const passive_endpoint& pep) -> fid_pep* { return pep.m_pep.get(); }

  /// ctor #1
  explicit passive_endpoint(boost::asio::io_context& io_context, const fabric& fabric)
  : m_fabric(fabric)
  , m_io_context(io_context)
  , m_eq(io_context, fabric)
  , m_pep(create_passive_endpoint(fabric, m_context))
  {
    // bind event queue to passive endpoint registering for connection requests
    bind(m_eq, passive_endpoint::eq_flag::connreq);
  }

  /// (default) ctor
  explicit passive_endpoint() = delete;

  /// copy ctor
  explicit passive_endpoint(const passive_endpoint& rh) = delete;

  /// move ctor
  explicit passive_endpoint(passive_endpoint&& rhs) = default;

  /// dtor
  ~passive_endpoint()
  {
  }

  enum class eq_flag : uint64_t {
    connreq = FI_CONNREQ
  };

  auto bind(const event_queue& eq, passive_endpoint::eq_flag flags) -> void
  {
    auto rc = fi_pep_bind(m_pep.get(), &get_wrapped_obj(eq)->fid, static_cast<uint64_t>(flags));
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed binding ofi event queue to ofi passive endpoint, reason: ", fi_strerror(rc));
  }

  /// Listen for connection requests
  template<typename CompletionHandler>
  auto listen(CompletionHandler&& handler) -> void
  {
    auto rc = fi_listen(m_pep.get());
    if (rc != 0)
      throw runtime_error("Failed listening on ofi passive_ep, reason: ", fi_strerror(rc));

    m_eq.read([&](eq::event event, fid_t handle, info&& info){
      if (event == eq::event::connreq) {
        handler(handle, std::move(info));
      } else {
        throw runtime_error("Unexpected event read from ofi event queue, ",
                            "expected: ", static_cast<uint32_t>(eq::event::connreq), " (event::connreq), ",
                            "got: ", static_cast<uint32_t>(event));
      }
    });
  }

  auto reject(fid_t handle) -> void
  {
    auto rc = fi_reject(m_pep.get(), handle, nullptr, 0);
    if (rc != 0)
      throw runtime_error("Failed rejecting connection request (", handle, ") on ofi passive endpoint, reason: ", fi_strerror(rc));
  }

  private:
  using fid_pep_deleter = std::function<void(fid_pep*)>;

  const fabric& m_fabric;
  fi_context m_context;
  boost::asio::io_context& m_io_context;
  event_queue m_eq;
  std::unique_ptr<fid_pep, fid_pep_deleter> m_pep;

  static auto create_passive_endpoint(const fabric& fabric, fi_context& context) -> std::unique_ptr<fid_pep, fid_pep_deleter> 
  {
    fid_pep* pep;
    auto rc = fi_passive_ep(get_wrapped_obj(fabric),
                            get_wrapped_obj(fabric.get_info()),
                            &pep,
                            &context);
    if (rc != 0)
      throw runtime_error("Failed creating ofi passive endpoint, reason: ", fi_strerror(rc));

    return {pep, [](fid_pep* pep){ fi_close(&pep->fid); }};
  }
}; /* struct passive_endpoint */

using pep = passive_endpoint;

} /* namespace asiofi */

#endif /* ASIOFI_ENDPOINT_HPP */
