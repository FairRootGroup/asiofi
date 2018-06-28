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
#include <asiofi/detail/handler_queue.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <rdma/fi_domain.h>
#include <iostream>

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


namespace detail
{

auto get_native_wait_fd(fid* obj) -> int
{
  int fd;
  auto rc = fi_control(obj, FI_GETWAIT, static_cast<void*>(&fd));
  if (rc != FI_SUCCESS)
    throw runtime_error("Failed retrieving native wait fd from ofi event queue, reason: ", fi_strerror(rc));

  return fd;
}

} /* namespace detail */

/**
 * @struct event_queue domain.hpp <include/asiofi/domain.hpp>
 * @brief Wraps ofi event queue
 */
struct event_queue
{
  /// get wrapped C object
  friend auto get_wrapped_obj(const event_queue& eq) -> fid_eq* { return eq.m_event_queue.get(); }

  explicit event_queue(boost::asio::io_context& io_context, const fabric& fabric)
  : m_fabric(fabric)
  , m_event_queue(create_event_queue(fabric, m_context))
  , m_io_context(io_context)
  , m_eq_fd(io_context, detail::get_native_wait_fd(&m_event_queue.get()->fid))
  {
  }

  event_queue() = delete;

  event_queue(const event_queue&) = delete;

  event_queue(event_queue&& rhs) = default;

  enum class event : uint32_t {
    connected = FI_CONNECTED,
    connreq = FI_CONNREQ,
    shutdown = FI_SHUTDOWN
  };

  template<typename CompletionHandler = std::function<void(event_queue::event, fid_t, info&&)>>
  struct read_op
  {
    explicit read_op(fid_eq* eq, CompletionHandler&& handler)
    : m_handler(handler)
    , m_event_queue(eq)
    {
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
          // not implemented yet, see ft_eq_readerr()
          throw runtime_error("Error pending on event queue, handling not yet implemented.");
        } else if (rc < 0) {
          throw runtime_error("Failed reading from event queue, reason: ", fi_strerror(rc));
        } else {
          auto event = static_cast<event_queue::event>(event_);

          if (event == event_queue::event::connreq) {
            m_handler(event, entry.fid, asiofi::info(entry.info));
          } else {
            m_handler(event, entry.fid, asiofi::info());
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
  auto read(CompletionHandler&& handler) -> void
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


/**
 * @struct completion_queue domain.hpp <include/asiofi/domain.hpp>
 * @brief Wraps ofi completion queue
 */
struct completion_queue
{
  /// get wrapped C object
  friend auto get_wrapped_obj(const completion_queue& cq) -> fid_cq* { return cq.m_completion_queue.get(); }

  enum class direction { rx, tx };

  explicit completion_queue(boost::asio::io_context& io_context, direction dir, const domain& domain)
  : m_domain(domain)
  , m_completion_queue(create_completion_queue(dir, domain.get_info(), domain, m_context))
  , m_io_context(io_context)
  , m_cq_fd(io_context, detail::get_native_wait_fd(&m_completion_queue->fid))
  {
  }

  completion_queue() = delete;

  completion_queue(const completion_queue&) = delete;

  completion_queue(completion_queue&&) = default;

  // enum class event : uint32_t {
    // connected = FI_CONNECTED,
    // connreq = FI_CONNREQ,
    // shutdown = FI_SHUTDOWN
  // };

  struct read_op
  {
    explicit read_op(fid_cq* cq, detail::handler_queue& queue)
    : m_read_handler_queue(queue)
    , m_completion_queue(cq)
    {
    }

    auto operator()(const boost::system::error_code& error = boost::system::error_code()) -> void
    {
      if (!error) {
        // struct fi_cq_data_entry {
        // void     *op_context; [> operation context <]
        // uint64_t flags;       [> completion flags <]
        // size_t   len;         [> size of received data <]
        // void     *buf;        [> receive data buffer <]
        // uint64_t data;        [> completion data <]
        // };
 
        // struct fi_cq_msg_entry {
        // void     *op_context; [> operation context <]
        // uint64_t flags;       [> completion flags <]
        // size_t   len;         [> size of received data <]
        // };

        // struct fi_cq_entry {
        // void     *op_context; [> operation context <]
        // };
        fi_cq_entry entry;
        // fi_cq_msg_entry entry;
        // fi_cq_data_entry entry;
        auto rc = fi_cq_sread(m_completion_queue, &entry, 1, nullptr, 100);
        if (rc == -FI_EAVAIL) {
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
          rc = fi_cq_readerr(m_completion_queue, &error, 0);
          assert(rc != -FI_EAGAIN); // should not happen
          if (rc < 0) {
            throw runtime_error("Failed reading error entry from completion queue, reason: ", fi_strerror(rc));
          } else {
            throw runtime_error("Failed completion event, reason: ", fi_cq_strerror(m_completion_queue,
                                                                                    error.prov_errno,
                                                                                    error.err_data, nullptr, 0));
          }
        } else if (rc < 0) {
          throw runtime_error("Failed reading from completion queue, reason: ", fi_strerror(rc));
        } else {
          // std::cout << "CQ entry read: op_context=" << entry.op_context << std::endl;
          // std::cout << "CQ entry read: op_context=" << entry.op_context << ", flags=" << entry.flags << ", len=" << entry.len << std::endl;
          // std::cout << "CQ entry read: op_context=" << entry.op_context << ", flags=" << entry.flags << ", len=" << entry.len << ", buf=" << entry.buf << ", data=" << entry.data << std::endl;
          m_read_handler_queue.front()->execute();
          m_read_handler_queue.pop();
        }
      } else {
        // TODO not implemented yet
      }
    }

    private:
    detail::handler_queue& m_read_handler_queue;
    fid_cq* m_completion_queue;
  };

  template<typename CompletionHandler>
  auto read(CompletionHandler&& handler) -> void
  {
    auto ex = boost::asio::get_associated_executor(handler, m_io_context);
    auto read_handler = boost::asio::bind_executor(ex, read_op(m_completion_queue.get(), m_read_handler_queue));
    // TODO preallocate
    m_read_handler_queue.push(detail::handler_queue::value_type(new detail::queued_handler<CompletionHandler>(std::move(handler))));

    auto wait_obj = &m_completion_queue.get()->fid;
    auto rc = fi_trywait(get_wrapped_obj(m_domain.get_fabric()), &wait_obj, 1);
    if (rc == FI_SUCCESS) {
      m_cq_fd.async_wait(boost::asio::posix::stream_descriptor::wait_read, std::move(read_handler));
    } else {
      boost::asio::dispatch(ex, std::move(read_handler));
    }
  }

  private:
  using fid_cq_deleter = std::function<void(fid_cq*)>;

  fi_context m_context;
  const domain& m_domain;
  std::unique_ptr<fid_cq, fid_cq_deleter> m_completion_queue;
  boost::asio::io_context& m_io_context;
  boost::asio::posix::stream_descriptor m_cq_fd;
  asiofi::detail::handler_queue m_read_handler_queue;

  static auto create_completion_queue(direction dir, const info& info, const domain& domain, fi_context& context)
  -> std::unique_ptr<fid_cq, fid_cq_deleter>
  {
    fid_cq* cq;
    fi_cq_attr cq_attr = {
      0,                 // size_t               size;      [> # entries for CQ <]
      0,                 // uint64_t             flags;     [> operation flags <]
      FI_CQ_FORMAT_CONTEXT,  // enum fi_cq_format    format;    [> completion format <]
      // FI_CQ_FORMAT_MSG,   // enum fi_cq_format    format;    [> completion format <]
      // FI_CQ_FORMAT_DATA,  // enum fi_cq_format    format;    [> completion format <]
      FI_WAIT_FD,        // enum fi_wait_obj     wait_obj;  [> requested wait object <]
      0,                 // int                  signaling_vector; [> interrupt affinity <]
      FI_CQ_COND_NONE,   // enum fi_cq_wait_cond wait_cond; [> wait condition format <]
      nullptr            // struct fid_wait*     wait_set;  [> optional wait set <]
    };

    if (dir == direction::rx) {
      cq_attr.size = get_wrapped_obj(info)->rx_attr->size;
    } else if (dir == direction::tx) {
      cq_attr.size = get_wrapped_obj(info)->tx_attr->size;
    }

    auto rc = fi_cq_open(get_wrapped_obj(domain),
                         &cq_attr,
                         &cq,
                         &context);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed opening ofi completion queue, reason: ", fi_strerror(rc));

    return {cq, [](fid_cq* cq){ fi_close(&cq->fid); }};
  }
}; /* struct completion_queue */

using cq = completion_queue;


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

  friend memory_region::access operator| (memory_region::access lhs, memory_region::access rhs)
  {
    using T = std::underlying_type<memory_region::access>::type;
    return static_cast<memory_region::access>(static_cast<T>(lhs) | static_cast<T>(rhs));
  }

  explicit memory_region(const domain& domain, boost::asio::mutable_buffer buffer, access access)
  : m_memory_region(std::move(create_memory_region(domain, buffer, access, m_context.get())))
  {
    // std::cout << "registered memory region: local_desc=" << desc() << " buf=" << buffer.data() << " size=" << buffer.size() << " access=0x" << std::hex << static_cast<uint64_t>(access) << std::dec << std::endl;
  }

  memory_region(const memory_region&) = default;

  memory_region(memory_region&&) = default;

  auto desc() -> void*
  {
    return fi_mr_desc(m_memory_region.get());
  }

  // ~memory_region()
  // {
    // if (m_memory_region.get() && (m_memory_region.use_count() == 1))
      // std::cout << "unregistering memory region: local_desc=" << desc() << " fid_mr*=" << m_memory_region.get() << std::endl;
  // }

  private:
  using fid_mr_deleter = std::function<void(fid_mr*)>;

  std::shared_ptr<fi_context> m_context;
  std::shared_ptr<fid_mr> m_memory_region;

  static auto create_memory_region(const domain& domain, boost::asio::mutable_buffer buffer, access access, fi_context* context)
  -> std::unique_ptr<fid_mr, fid_mr_deleter>
  {
    fid_mr* mr;
    auto rc = fi_mr_reg(get_wrapped_obj(domain), buffer.data(), buffer.size(), static_cast<uint64_t>(access), 0, 0, 0, &mr, context);
    if (rc != 0)
      throw runtime_error("Failed registering ofi memory region, reason: ", fi_strerror(rc));

    return {mr, [](fid_mr* mr){
      fi_close(&mr->fid);
      // std::cout << "fi_close: fid_mr*=" << mr << std::endl;
    }};
  }
}; /* struct memory_region */

using mr = memory_region;

} /* namespace asiofi */

#endif /* ASIOFI_DOMAIN_HPP */
