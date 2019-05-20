/********************************************************************************
 * Copyright (C) 2018-2019 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_SEMAPHORE_HPP
#define ASIOFI_SEMAPHORE_HPP

#include <asiofi/errno.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/executor.hpp>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <folly/Function.h>
#include <functional>
#include <mutex>

namespace asiofi {
  /**
   * @struct basic_unsynchronized_semaphore semaphore.hpp <asiofi/semaphore.hpp>
   * @brief A simple asio-enabled semaphore
   *
   * Semaphore:
   * - NOT thread-safe
   */
  template <typename Executor = boost::asio::executor>
  struct basic_unsynchronized_semaphore
  {
    using executor_type = Executor;

    explicit basic_unsynchronized_semaphore(const executor_type& ex,
                                      std::size_t initial_count = 1)
      : m_executor(ex)
      , m_count(initial_count)
      , m_handler(nullptr)
    {}

    template<typename CompletionHandler>
    auto async_wait(CompletionHandler&& handler) -> void
    {
      if (m_count > 0) {
        --m_count;
        boost::asio::dispatch(m_executor, std::move(handler));
      } else {
        if (!m_handler) {
          m_handler = std::move(handler);
        } else {
          throw runtime_error(
            "Cannot initiate semaphore::async_wait twice at the same time.");
        }
      }
    }

    auto wait() -> void
    {
      if (m_count > 0) {
        --m_count;
      } else {
        throw runtime_error("Cannot wait on semaphore, its count is 0.");
      }
    }

    template<typename CompletionHandler>
    auto async_signal(CompletionHandler&& handler) -> void
    {
      if (m_handler) {
        // complete the waiting wait operation, then complete this operation
        auto tmp = std::move(m_handler);
        m_handler = nullptr;
        boost::asio::dispatch(m_executor,
                              [waiting_completion = std::move(tmp),
                               current_completion = std::move(handler)]() mutable {
                                waiting_completion();
                                current_completion();
                              });
      } else {
        ++m_count;
        boost::asio::dispatch(m_executor, std::move(handler));
      }
    }

    auto signal() -> void
    {
      if (m_handler) {
        auto tmp = std::move(m_handler);
        m_handler = nullptr;
        boost::asio::dispatch(m_executor, std::move(tmp));
      } else {
        ++m_count;
      }
    }

    auto get_count() -> std::size_t
    {
      return m_count;
    }

  private:
    executor_type m_executor;
    std::size_t m_count;
    folly::Function<void()> m_handler;
  };

  using unsynchronized_semaphore = basic_unsynchronized_semaphore<>;

  /**
   * @struct basic_synchronized_semaphore semaphore.hpp <asiofi/semaphore.hpp>
   * @brief A simple asio-enabled semaphore
   *
   * Semaphore:
   * - thread-safe
   */
  template <typename Executor = boost::asio::executor>
  struct basic_synchronized_semaphore
  {
    using executor_type = Executor;

    explicit basic_synchronized_semaphore(const executor_type& ex,
                                    std::size_t initial_count = 1)
      : m_executor(ex)
      , m_count(initial_count)
      , m_handler(nullptr)
    {}

    template<typename CompletionHandler>
    auto async_wait(CompletionHandler&& handler) -> void
    {
      std::unique_lock<std::mutex> lk(m_mutex);

      if (m_count > 0) {
        --m_count;
        lk.unlock();
        boost::asio::dispatch(m_executor, std::move(handler));
      } else {
        if (!m_handler) {
          m_handler = std::move(handler);
        } else {
          throw runtime_error(
            "Cannot initiate semaphore::async_wait twice at the same time.");
        }
      }
    }

    auto wait() -> void
    {
      std::unique_lock<std::mutex> lk(m_mutex);

      if (m_count > 0) {
        --m_count;
      } else {
        m_cv.wait(lk, [this] { return m_count > 0; });
        --m_count;
      }
    }

    template<typename CompletionHandler>
    auto async_signal(CompletionHandler&& handler) -> void
    {
      std::unique_lock<std::mutex> lk(m_mutex);

      if (m_handler) {
        auto waiting = std::move(m_handler);
        m_handler = nullptr;
        lk.unlock();
        // complete the waiting wait operation, then complete this operation
        boost::asio::dispatch(m_executor,
                              [waiting_completion = std::move(waiting),
                               current_completion = std::move(handler)]() mutable {
                                waiting_completion();
                                current_completion();
                              });
      } else {
        ++m_count;
        lk.unlock();
        m_cv.notify_one();
        boost::asio::dispatch(m_executor, std::move(handler));
      }
    }

    auto signal() -> void
    {
      std::unique_lock<std::mutex> lk(m_mutex);

      if (m_handler) {
        auto waiting = std::move(m_handler);
        m_handler = nullptr;
        lk.unlock();
        // complete the waiting signal operation
        boost::asio::dispatch(m_executor, std::move(waiting));
      } else {
        ++m_count;
        lk.unlock();
        m_cv.notify_one();
      }
    }

    auto get_count() -> std::size_t
    {
      std::unique_lock<std::mutex> lk(m_mutex);
      return m_count;
    }

  private:
    executor_type m_executor;
    std::size_t m_count;
    folly::Function<void()> m_handler;
    std::mutex m_mutex;
    std::condition_variable m_cv;
  };

  using synchronized_semaphore = basic_synchronized_semaphore<>;
}   // namespace asiofi

#endif /* ifndef ASIOFI_SEMAPHORE_HPP */  
