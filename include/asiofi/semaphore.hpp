/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_SEMAPHORE_HPP
#define ASIOFI_SEMAPHORE_HPP

#include <asiofi/errno.hpp>
#include <atomic>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <cassert>
#include <cstdint>
#include <sys/eventfd.h>

namespace asiofi {

  /**
   * @struct semaphore semaphore.hpp <asiofi/semaphore.hpp>
   * @brief Implements a simple semaphore with asynchronous operations
   *
   * Asio-enabled semaphore. The Linux implementation is based on the eventfd(2) API.
   */
  struct semaphore
  {
    explicit semaphore(boost::asio::io_context& ioc, uint64_t initial_value = 1)
      : m_strand(ioc)
      , m_descriptor(m_strand.context(), create_eventfd(initial_value))
      , m_read_buffer(0)
      , m_signal_buffer(0)
      , m_cnt(initial_value)
    {}

    template<typename CompletionHandler>
    auto async_wait(CompletionHandler&& handler) -> void
    {
      boost::asio::mutable_buffer mb(&m_read_buffer, 8);
      auto ex = boost::asio::get_associated_executor(handler, m_strand);

      m_descriptor.async_read_some(
        mb,
        boost::asio::bind_executor(
          ex,
          [&, handler_ = std::move(handler)](const boost::system::error_code& ec,
                                             size_t bytes_read) {
            if (!ec) {
              assert(bytes_read == 8);
              (void)bytes_read;
              assert(static_cast<uint64_t>(mb.data()) == 1);
              --m_cnt;
            }
            handler_(ec);
          }));
    }

    auto wait() -> void
    {
      boost::asio::mutable_buffer mb(&m_read_buffer, 8);

      m_descriptor.read_some(mb);
    }

    template<typename CompletionHandler>
    auto async_signal(CompletionHandler&& handler) -> void
    {
      m_signal_buffer = 1;
      boost::asio::const_buffer cb(&m_signal_buffer, 8);
      auto ex = boost::asio::get_associated_executor(handler, m_strand);

      m_descriptor.async_write_some(
        cb,
        boost::asio::bind_executor(
          ex,
          [&, handler_ = std::move(handler)](const boost::system::error_code& ec,
                                             size_t bytes_written) {
            if (!ec) {
              assert(bytes_written == 8);
              (void)bytes_written;
              ++m_cnt;
            }
            handler_(ec);
          }));
    }

    auto signal() -> void
    {
      m_signal_buffer = 1;
      boost::asio::const_buffer cb(&m_signal_buffer, 8);

      m_descriptor.write_some(cb);
    }

    auto get_value() -> uint64_t { return m_cnt; }

  private:
    boost::asio::io_context::strand m_strand;
    boost::asio::posix::stream_descriptor m_descriptor;
    uint64_t m_read_buffer, m_signal_buffer;
    std::atomic<uint64_t> m_cnt;

    auto create_eventfd(uint64_t initial_value) -> int {
      int ret = eventfd(initial_value, EFD_SEMAPHORE);
      if(ret == -1) {
        throw runtime_error("Could not create eventfd(2)");
      }

      return ret;
    }
  };
}   // namespace asiofi

#endif /* ifndef ASIOFI_SEMAPHORE_HPP */  
