/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_EVENT_QUEUE_HPP
#define ASIOFI_EVENT_QUEUE_HPP

#include <asiofi/detail/get_native_wait_fd.hpp>
#include <asiofi/fabric.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <rdma/fi_domain.h>

namespace asiofi
{
  /**
   * @struct event_queue event_queue.hpp <include/asiofi/event_queue.hpp>
   * @brief Wraps fid_eq
   *
   * TODO implement analog to asiofi::completion_queue
   */
  struct event_queue
  {
    /// get wrapped C object
    friend auto get_wrapped_obj(const event_queue& eq) -> fid_eq*
    {
      return eq.m_event_queue.get();
    }

    explicit event_queue(boost::asio::io_context& io_context, const fabric& fabric)
      : m_fabric(fabric)
      , m_event_queue(create_event_queue(fabric, m_context))
      , m_io_context(io_context)
      , m_eq_fd(io_context, detail::get_native_wait_fd(&m_event_queue.get()->fid))
    {}

    event_queue() = delete;

    event_queue(const event_queue&) = delete;

    event_queue(event_queue&& rhs) = default;

    enum class event : uint32_t
    {
      connected = FI_CONNECTED,
      connreq = FI_CONNREQ,
      shutdown = FI_SHUTDOWN,
      connrefused
    };

    template<typename CompletionHandler = std::function<void(event_queue::event, info&&)>>
    struct read_op
    {
      explicit read_op(fid_eq* eq, CompletionHandler&& handler)
      : m_handler(handler)
      , m_event_queue(eq)
      {
      }

      auto reader_handle_error() -> void
      {
        // struct fi_eq_err_entry {
	      // fid_t    fid;         /* fid associated with error */
	      // void     *context;    /* operation context */
        // uint64_t data;        /* completion-specific data */
        // int      err;         /* positive error code */
        // int      prov_errno;  /* provider error code */
        // void     *err_data;   /* additional error data */
        // size_t   err_data_size; /* size of err_data */
        // };
        fi_eq_err_entry error;
        error.err_data_size = 0;
        auto rc = fi_eq_readerr(m_event_queue, &error, 0);
        assert(rc != -FI_EAGAIN); // should not happen
        if (rc < 0) {
          throw runtime_error(rc, "Failed reading error entry from event queue");
        } else {
          if (error.err == FI_ECONNREFUSED) {
            m_handler(event_queue::event::connrefused, asiofi::info());
          } else {
            throw runtime_error(
              "Failed event, reason: ",
              fi_eq_strerror(
                m_event_queue, error.prov_errno, error.err_data, nullptr, 0));
          }
        }
      }

      auto operator()(const boost::system::error_code& error = boost::system::error_code()) -> void
      {
        if (!error) {
          // fi_eq_cm_entry {
          // fid_t            fid;        [> fid associated with request <]
          // struct fi_info  *info;       [> endpoint information <]
          // uint8_t          data[];     [> app connection data <]
          // };
          fi_eq_cm_entry entry;
          uint32_t event_;
          auto rc = fi_eq_sread(m_event_queue, &event_, &entry, sizeof(entry), 100, 0);
          if (rc == -FI_EAVAIL) {
            reader_handle_error();
          } else if (rc < 0) {
            throw runtime_error(rc, "Failed reading from event queue");
          } else {
            auto event = static_cast<event_queue::event>(event_);

            if (event == event_queue::event::connreq) {
              m_handler(event, asiofi::info(entry.info));
            } else {
              m_handler(event, asiofi::info());
            }
          }
        } else {
          // not implemented yet
        }
      }

      private:
      CompletionHandler m_handler;
      fid_eq* m_event_queue;
    };

    template<typename CompletionHandler>
    auto async_read(CompletionHandler&& handler) -> void
    {
      auto wait_obj = &m_event_queue.get()->fid;
      auto rc = fi_trywait(get_wrapped_obj(m_fabric), &wait_obj, 1);
      auto ex = boost::asio::get_associated_executor(handler, m_io_context);
      auto read_handler = boost::asio::bind_executor(ex, read_op<>(m_event_queue.get(), std::move(handler)));
      if (rc == FI_SUCCESS) {
        m_eq_fd.async_wait(boost::asio::posix::stream_descriptor::wait_read, std::move(read_handler));
      } else {
        boost::asio::dispatch(ex, std::move(read_handler));
      }
    }

    private:
    using fid_eq_deleter = std::function<void(fid_eq*)>;

    fi_context m_context;
    const fabric& m_fabric;
    std::unique_ptr<fid_eq, fid_eq_deleter> m_event_queue;
    boost::asio::io_context& m_io_context;
    boost::asio::posix::stream_descriptor m_eq_fd;

    static auto create_event_queue(const fabric& fabric, fi_context& context) -> std::unique_ptr<fid_eq, fid_eq_deleter>
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

      return {eq, [](fid_eq* eq){ fi_close(&eq->fid); }};
    }
  }; /* struct event_queue */

  using eq = event_queue;

} /* namespace asiofi */

#endif /* ASIOFI_EVENT_QUEUE_HPP */
