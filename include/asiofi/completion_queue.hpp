/********************************************************************************
 *    Copyright (C) 2019 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_COMPLETION_QUEUE_HPP
#define ASIOFI_COMPLETION_QUEUE_HPP

#include <asiofi/errno.hpp>
#include <asiofi/domain.hpp>
#include <asiofi/detail/get_native_wait_fd.hpp>
#include <asiofi/detail/handler_queue.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <memory>
#include <rdma/fi_domain.h>
#include <utility>

namespace asiofi
{
  /**
   * @struct completion_queue completion_queue.hpp <include/asiofi/completion_queue.hpp>
   * @brief Wraps ofi completion queue
   */
  struct completion_queue
  {
    /// get wrapped C object
    friend auto get_wrapped_obj(const completion_queue& cq) -> fid_cq*
    {
      return cq.m_completion_queue.get();
    }

    enum class direction
    {
      rx,
      tx
    };

    explicit completion_queue(boost::asio::io_context& ctx,
                              direction dir,
                              const domain& domain)
      : m_domain(domain)
      , m_completion_queue(
          create_completion_queue(dir, domain.get_info(), domain, m_context))
      , m_io_context(ctx)
      , m_cq_fd(m_io_context, detail::get_native_wait_fd(&m_completion_queue->fid))
    {
      post_reader();   // Start reading CQ events
    }

    completion_queue() = delete;

    completion_queue(const completion_queue&) = delete;

    completion_queue(completion_queue&&) = default;

    // enum class event : uint32_t {
    // connected = FI_CONNECTED,
    // connreq = FI_CONNREQ,
    // shutdown = FI_SHUTDOWN
    // };

    template<typename CompletionHandler>
    auto async_read(CompletionHandler&& handler, std::unique_ptr<fi_context> ctx) -> void
    {
      m_read_handler_queue.push(
        detail::handler_queue::value_type(new detail::queued_handler<CompletionHandler>(
          std::forward<CompletionHandler>(handler), std::move(ctx))));
    }

  private:
    using fid_cq_deleter = std::function<void(fid_cq*)>;

    fi_context m_context;
    const domain& m_domain;
    std::unique_ptr<fid_cq, fid_cq_deleter> m_completion_queue;
    boost::asio::io_context& m_io_context;
    boost::asio::posix::stream_descriptor m_cq_fd;
    asiofi::detail::handler_queue m_read_handler_queue;

    auto post_reader() -> void
    {
      auto wait_obj = &m_completion_queue.get()->fid;
      auto rc = fi_trywait(get_wrapped_obj(m_domain.get_fabric()), &wait_obj, 1);
      if (rc == FI_SUCCESS) {
        // std::cout << "wait on fd" << std::endl;
        m_cq_fd.async_wait(
          boost::asio::posix::stream_descriptor::wait_read,
          std::move(
            std::bind(&completion_queue::reader, this, std::placeholders::_1, true)));
        // call trywait again to make sure, we do not miss the notification
        // reader(boost::system::error_code(), false);
        // rc = fi_trywait(get_wrapped_obj(m_domain.get_fabric()), &wait_obj, 1);
        // assert(rc == FI_SUCCESS);
      } else {
        // std::cout << "call" << std::endl;
        // reader(boost::system::error_code());
        // std::cout << "post" << std::endl;
        boost::asio::post(
          m_io_context,
          std::move(std::bind(
            &completion_queue::reader, this, boost::system::error_code(), true)));
      }
    }

    auto reader_handle_error() -> void
    {
      // struct fi_cq_err_entry {
      // void     *op_context; /* operation context */
      // uint64_t flags;       /* completion flags */
      // size_t   len;         /* size of received data */
      // void     *buf;        /* receive data buffer */
      // uint64_t data;        /* completion data */
      // uint64_t tag;         /* message tag */
      // size_t   olen;        /* overflow length */
      // int      err;         /* positive error code */
      // int      prov_errno;  /* provider error code */
      // void    *err_data;    /*  error data */
      // size_t   err_data_size; /* size of err_data */
      // };
      fi_cq_err_entry error;
      error.err_data_size = 0;
      auto rc = fi_cq_readerr(m_completion_queue.get(), &error, 0);
      assert(rc != -FI_EAGAIN);   // should not happen
      if (rc < 0) {
        throw runtime_error("Failed reading error entry from completion queue, reason: ",
                            fi_strerror(rc));
      } else {
        throw runtime_error(
          "Failed completion event, reason: ",
          fi_cq_strerror(
            m_completion_queue.get(), error.prov_errno, error.err_data, nullptr, 0));
      }
    }

    auto reader(const boost::system::error_code& error, bool continuation = true) -> void
    {
      if (!error) {
        // struct fi_cq_entry {
        // void     *op_context; [> operation context <]
        // };
        fi_cq_entry entry[1];
        ssize_t rc;
        while ((rc = fi_cq_read(m_completion_queue.get(), &entry, 1)) != -FI_EAGAIN) {
          if (rc == -FI_EAVAIL) {
            reader_handle_error();
          } else if (rc < 0) {
            throw runtime_error("Failed reading from completion queue, reason: ",
                                fi_strerror(rc));
          } else {
            for (ssize_t i = 0; i < rc; ++i) {
              // std::cout << "CQ entry read: op_context=" << entry.op_context <<
              // std::endl;
              if (m_read_handler_queue.empty()) {
                throw runtime_error(
                  "Received CQ event, but no completion handler is queued");
              }
              assert(m_read_handler_queue.front()->context() == entry[i].op_context);
              m_read_handler_queue.front()->execute();
              m_read_handler_queue.pop();
            }
          }
        }
        if (continuation) {
          post_reader();
        }
      } else {
        // TODO is there anything to do here? We might end up here, when the asio event
        // loop is stopped.
      }
    }

    static auto create_completion_queue(direction dir,
                                        const info& info,
                                        const domain& domain,
                                        fi_context& context)
      -> std::unique_ptr<fid_cq, fid_cq_deleter>
    {
      fid_cq* cq;
      fi_cq_attr cq_attr = {
        0,                      // size_t               size;      [> # entries for CQ <]
        0,                      // uint64_t             flags;     [> operation flags <]
        FI_CQ_FORMAT_CONTEXT,   // enum fi_cq_format    format;    [> completion format <]
        // FI_CQ_FORMAT_MSG,   // enum fi_cq_format    format;    [> completion format <]
        // FI_CQ_FORMAT_DATA,  // enum fi_cq_format    format;    [> completion format <]
        FI_WAIT_FD,   // enum fi_wait_obj     wait_obj;  [> requested wait object <]
        0,            // int                  signaling_vector; [> interrupt affinity <]
        FI_CQ_COND_NONE,   // enum fi_cq_wait_cond wait_cond; [> wait condition format <]
        nullptr            // struct fid_wait*     wait_set;  [> optional wait set <]
      };

      if (dir == direction::rx) {
        cq_attr.size = get_wrapped_obj(info)->rx_attr->size;
      } else if (dir == direction::tx) {
        cq_attr.size = get_wrapped_obj(info)->tx_attr->size;
      }

      auto rc = fi_cq_open(get_wrapped_obj(domain), &cq_attr, &cq, &context);
      if (rc != FI_SUCCESS)
        throw runtime_error("Failed opening ofi completion queue, reason: ",
                            fi_strerror(rc));

      return {cq, [](fid_cq* cq) { fi_close(&cq->fid); }};
    }
  }; /* struct completion_queue */

  using cq = completion_queue;

}   // namespace asiofi

#endif /* ifndef ASIOFI_COMPLETION_QUEUE_HPP */
