/********************************************************************************
 *    Copyright (C) 2019 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_PRIORITY_SCHEDULER_HPP
#define ASIOFI_PRIORITY_SCHEDULER_HPP

#include <algorithm>
#include <asiofi/errno.hpp>
#include <boost/asio/execution_context.hpp>
#include <condition_variable>
#include <folly/Function.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

namespace asiofi {

  /**
   * @struct priority_queue priority_scheduler.hpp <asiofi/priority_scheduler.hpp>
   * @brief An alternative to std::priority_queue that supports a "moving pop".
   */
  template<typename T, typename Compare = std::less<T>>
  struct priority_queue
  {
    explicit priority_queue(const Compare& compare = Compare())
      : m_compare(compare)
    {}

    template<typename ... Args>
    auto emplace(Args&& ... args) -> void
    {
      m_elements.emplace_back(std::forward<Args>(args)...);
      std::push_heap(m_elements.begin(), m_elements.end(), m_compare);
    }

    auto pop() -> T&&
    {
      std::pop_heap(m_elements.begin(), m_elements.end(), m_compare);
      T result(std::move(m_elements.back()));
      m_elements.pop_back();
      return std::move(result);
    }

    auto empty() const -> bool { return m_elements.empty(); };

  private:
    std::vector<T> m_elements;
    Compare m_compare;
  };

  /**
   * @struct priority_scheduler priority_scheduler.hpp <asiofi/priority_scheduler.hpp>
   * @brief An asio execution_context which uses a priority queue as work queue
   *
   * Based on https://github.com/boostorg/asio/blob/master/example/cpp11/executors/priority_scheduler.cpp
   */
  struct priority_scheduler : boost::asio::execution_context
  {
  public:
    // A class that satisfies the Executor requirements.
    class executor_type
    {
    public:
      executor_type(priority_scheduler& ctx, int pri) noexcept
        : m_context(ctx)
        , m_priority(pri)
      {}

      priority_scheduler& context() const noexcept { return m_context; }

      void on_work_started() const noexcept
      {
        // This executor doesn't count work. Instead, the scheduler simply runs
        // until explicitly stopped.
      }

      void on_work_finished() const noexcept
      {
        // This executor doesn't count work. Instead, the scheduler simply runs
        // until explicitly stopped.
      }

      template<class Func, class Alloc>
      void dispatch(Func&& f, const Alloc& a) const
      {
        post(std::forward<Func>(f), a);
      }

      template<class Func, class Alloc>
      void post(Func f, const Alloc& a) const
      {
        std::lock_guard<std::mutex> lock(m_context.m_mutex);
        m_context.m_queue.emplace(m_priority, std::move(f));
        m_context.m_condition.notify_one();
      }

      template<class Func, class Alloc>
      void defer(Func&& f, const Alloc& a) const
      {
        post(std::forward<Func>(f), a);
      }

      friend auto operator==(const executor_type& a, const executor_type& b) noexcept -> bool
      {
        return &a.m_context == &b.m_context;
      }

      friend auto operator!=(const executor_type& a, const executor_type& b) noexcept -> bool
      {
        return &a.m_context != &b.m_context;
      }

    private:
      priority_scheduler& m_context;
      int m_priority;
    };

    executor_type get_executor(int pri = 0) noexcept
    {
      return executor_type(*const_cast<priority_scheduler*>(this), pri);
    }

    void run()
    {
      std::unique_lock<std::mutex> lock(m_mutex);

      for (;;) {
        m_condition.wait(lock, [&] { return m_stopped || !m_queue.empty(); });
        if (m_stopped)
          return;
        auto handler(m_queue.pop());
        lock.unlock();
        handler();
        lock.lock();
      }
    }

    void stop()
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_stopped = true;
      m_condition.notify_all();
    }

  private:
    struct prioritized_handler
    {
      template<typename Handler>
      prioritized_handler(int pri, Handler&& f)
        : m_priority(pri)
        , m_handler(std::move(f))
      {}

      auto operator()() -> void { m_handler(); }

      friend auto operator<(const prioritized_handler& left,
                            const prioritized_handler& right) -> bool
      {
        return left.m_priority < right.m_priority;
      }

    private:
      int m_priority;
      folly::Function<void()> m_handler;
    };

    std::mutex m_mutex;
    std::condition_variable m_condition;
    priority_queue<prioritized_handler> m_queue;
    bool m_stopped = false;
  };
}   // namespace asiofi

#endif /* ifndef ASIOFI_PRIORITY_SCHEDULER_HPP */
