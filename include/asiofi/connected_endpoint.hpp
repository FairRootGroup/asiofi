/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_CONNECTED_ENDPOINT_HPP
#define ASIOFI_CONNECTED_ENDPOINT_HPP

#include <arpa/inet.h>
#include <asiofi/domain.hpp>
#include <asiofi/errno.hpp>
#include <asiofi/event_queue.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <cassert>
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_endpoint.h>
#include <utility>

namespace asiofi {
  /**
   * @struct connected_endpoint conntected_endpoint.hpp <asiofi/connected_endpoint.hpp>
   * @brief Wraps fid_ep and connected mode operations
   */
  struct connected_endpoint
  {
    /// get wrapped C object
    friend auto get_wrapped_obj(const connected_endpoint& ep) -> fid_ep*
    {
      return ep.m_connected_endpoint.get();
    }

    /// ctor #1
    explicit connected_endpoint(boost::asio::io_context& io_context,
                                const domain& domain,
                                const info& info)
      : m_io_context(io_context)
      , m_domain(domain)
      , m_eq(m_io_context, domain.get_fabric())
      , m_rx_strand(m_io_context)
      , m_tx_strand(m_io_context)
      , m_rx_cq(m_rx_strand, cq::direction::rx, domain)
      , m_tx_cq(m_tx_strand, cq::direction::tx, domain)
      , m_connected_endpoint(create_connected_endpoint(domain, info, m_context))
    {
      bind(m_eq);
      bind(m_rx_cq, connected_endpoint::cq_flag::recv);
      bind(m_tx_cq, connected_endpoint::cq_flag::transmit);
    }

    /// ctor #2
    explicit connected_endpoint(boost::asio::io_context& io_context, const domain& domain)
      : connected_endpoint(io_context, domain, domain.get_info())
    {}

    connected_endpoint() = delete;

    connected_endpoint(const connected_endpoint& rh) = delete;

    connected_endpoint(connected_endpoint&& rhs) = default;

    auto bind(const event_queue& eq) -> void
    {
      auto rc = fi_ep_bind(m_connected_endpoint.get(), &get_wrapped_obj(eq)->fid, 0);
      if (rc != FI_SUCCESS)
        throw runtime_error(rc,
                            "Failed binding ofi event queue to ofi connected_endpoint");
    }

    enum class cq_flag : uint64_t
    {
      transmit = FI_TRANSMIT,
      recv = FI_RECV,
      selective_completion = FI_SELECTIVE_COMPLETION
    };

    auto bind(const completion_queue& cq, connected_endpoint::cq_flag flag) -> void
    {
      auto rc = fi_ep_bind(m_connected_endpoint.get(),
                           &get_wrapped_obj(cq)->fid,
                           static_cast<uint64_t>(flag));
      if (rc != FI_SUCCESS)
        throw runtime_error(
          "Failed binding ofi completion queue to ofi connected_endpoint, reason: ",
          fi_strerror(rc));
    }

    /// transition endpoint to enabled state
    auto enable() -> void
    {
      auto rc = fi_enable(m_connected_endpoint.get());
      if (rc != FI_SUCCESS)
        throw runtime_error(
          "Failed transitioning ofi connected_endpoint to enabled state, reason: ",
          fi_strerror(rc));
    }

    template<typename CompletionHandler>
    auto connect(sockaddr_in addr, CompletionHandler&& handler) -> void
    {
      auto rc = fi_connect(m_connected_endpoint.get(),
                           &addr,
                           nullptr,
                           0);
      if (rc != FI_SUCCESS)
        throw runtime_error(rc,
                            "Failed initiating connection to ",
                            "",   // TODO print addr
                            " on ofi connected_endpoint");

      m_eq.read(
        [&, _handler = std::move(handler)](eq::event event, fid_t handle, info&& info) {
          if (event == eq::event::connected) {
            _handler();
          } else {
            throw runtime_error(
              "Unexpected event read from ofi event queue, expected FI_CONNECTED, got: ",
              static_cast<uint32_t>(event));
          }
        });
    }

    template<typename CompletionHandler>
    auto connect(CompletionHandler&& handler) -> void
    {
      connect(*static_cast<const sockaddr_in*>(
                get_wrapped_obj(m_domain.get_fabric().get_info())->dest_addr),
              std::move(handler));
    }

    template<typename CompletionHandler>
    auto accept(CompletionHandler&& handler) -> void
    {
      auto rc = fi_accept(m_connected_endpoint.get(), nullptr, 0);
      if (rc != FI_SUCCESS)
        throw runtime_error("Failed accepting connection, reason: ", fi_strerror(rc));

      m_eq.read(
        [&, _handler = std::move(handler)](eq::event event, fid_t handle, info&& info) {
          if (event == eq::event::connected) {
            _handler();
          } else {
            throw runtime_error(
              "Unexpected event read from ofi event queue, expected FI_CONNECTED, got: ",
              static_cast<uint32_t>(event));
          }
        });
    }

    template<typename CompletionHandler>
    auto send(boost::asio::mutable_buffer buffer,
              void* mr_desc,
              CompletionHandler&& handler) -> void
    {
      fi_addr_t dummy_addr;
      auto ctx =
        std::unique_ptr<fi_context>(new fi_context{nullptr, nullptr, nullptr, nullptr});

      // std::cout << "fi_send: buf=" << buffer.data()
                       // << ", len=" << buffer.size()
                       // << ", desc=" << mr_desc
                       // << ", ctx=" << ctx.get() << std::endl;
      auto rc = fi_send(m_connected_endpoint.get(),
                        buffer.data(),
                        buffer.size(),
                        mr_desc,
                        dummy_addr,
                        ctx.get());
      if (rc != FI_SUCCESS) {
        throw runtime_error(
          "Failed posting a TX buffer on ofi connected_endpoint, reason: ",
          fi_strerror(rc));
      }

      auto ex = boost::asio::get_associated_executor(handler, m_tx_strand);
      m_tx_cq.read(
        boost::asio::bind_executor(
          ex, [=, handler2 = std::move(handler)]() mutable { handler2(buffer); }),
        std::move(ctx));
    }

    template<typename CompletionHandler>
    auto send(boost::asio::mutable_buffer buffer, CompletionHandler&& handler) -> void
    {
      send(std::move(buffer), nullptr, std::move(handler));
    }

    template<typename CompletionHandler>
    auto recv(boost::asio::mutable_buffer buffer,
              void* mr_desc,
              CompletionHandler&& handler) -> void
    {
      fi_addr_t dummy_addr;
      auto ctx =
        std::unique_ptr<fi_context>(new fi_context{nullptr, nullptr, nullptr, nullptr});

      // std::cout << "fi_recv: buf=" << buffer.data()
      //                  << ", len=" << buffer.size()
      //                  << ", desc=" << mr_desc
      //                  << ", ctx=" << ctx.get() << std::endl;
      auto rc = fi_recv(m_connected_endpoint.get(),
                        buffer.data(),
                        buffer.size(),
                        mr_desc,
                        dummy_addr,
                        ctx.get());
      if (rc != FI_SUCCESS) {
        throw runtime_error(
          "Failed posting a RX buffer on ofi connected_endpoint, reason: ",
          fi_strerror(rc));
      }

      auto ex = boost::asio::get_associated_executor(handler, m_rx_strand);
      m_rx_cq.read(
        boost::asio::bind_executor(
          ex, [=, handler2 = std::move(handler)]() mutable { handler2(buffer); }),
        std::move(ctx));
    }

    template<typename CompletionHandler>
    auto recv(boost::asio::mutable_buffer buffer, CompletionHandler&& handler) -> void
    {
      recv(std::move(buffer), nullptr, std::move(handler));
    }

    auto shutdown() -> void
    {
      auto rc = fi_shutdown(m_connected_endpoint.get(), 0);
      if (rc != FI_SUCCESS) {
        throw runtime_error("Failed shutting down ofi connected_endpoint, reason: ",
                            fi_strerror(rc));
      }
    }

    auto get_local_address() -> sockaddr_in
    {
      sockaddr_in addr;
      size_t addrlen = sizeof(sockaddr_in);
      auto rc = fi_getname(&(m_connected_endpoint.get()->fid), &addr, &addrlen);
      if (rc != FI_SUCCESS)
          throw runtime_error("Failed retrieving native address from ofi connected_endpoint, reason: ", fi_strerror(rc));
      assert(addrlen == sizeof(sockaddr_in));

      return addr;
    }

    auto set_local_address(sockaddr_in addr) -> void
    {
      auto rc = fi_setname(&(m_connected_endpoint.get()->fid), &addr, sizeof(sockaddr_in));
      if (rc != FI_SUCCESS)
          throw runtime_error("Failed setting native address on ofi connected_endpoint, reason: ", fi_strerror(rc));
    }

  private:
    using fid_ep_deleter = std::function<void(fid_ep*)>;

    fi_context m_context;
    boost::asio::io_context& m_io_context;
    const domain& m_domain;
    event_queue m_eq;
    boost::asio::io_context::strand m_rx_strand, m_tx_strand;
    completion_queue m_rx_cq, m_tx_cq;
    std::unique_ptr<fid_ep, fid_ep_deleter> m_connected_endpoint;

    static auto create_connected_endpoint(const domain& domain,
                                          const info& info,
                                          fi_context& context)
      -> std::unique_ptr<fid_ep, fid_ep_deleter>
    {
      fid_ep* ep;
      auto rc = fi_endpoint(
        get_wrapped_obj(domain), get_wrapped_obj(info), &ep, &context);
      if (rc != 0) {
        throw runtime_error("Failed creating ofi connected_endpoint, reason: ",
                            fi_strerror(rc));
      }

      return {ep, [](fid_ep* ep) { fi_close(&ep->fid); }};
    }
  }; /* struct connected_endpoint */

  using cep = connected_endpoint;

} /* namespace asiofi */

#endif /* ASIOFI_CONNECTED_ENDPOINT_HPP */
