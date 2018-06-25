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
#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
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
  friend auto get_wrapped_obj(const endpoint& ep) -> fid_ep* { return ep.m_endpoint; }

  /// ctor #1
  explicit endpoint(boost::asio::io_context& io_context, const domain& domain, const info& info)
  : m_io_context(io_context)
  , m_domain(domain)
  , m_endpoint(create_endpoint(domain, info, m_context))
  , m_eq(create_event_queue(domain.get_fabric(), m_context))
  , m_eq_fd(io_context, get_native_wait_fd(&m_eq->fid))
  , m_rx_cq(create_completion_queue(direction::rx, domain.get_info(), domain, m_context))
  , m_tx_cq(create_completion_queue(direction::tx, domain.get_info(), domain, m_context))
  , m_rx_cq_fd(io_context, get_native_wait_fd(&m_rx_cq->fid))
  , m_tx_cq_fd(io_context, get_native_wait_fd(&m_tx_cq->fid))
  , m_rx_strand(io_context)
  , m_tx_strand(io_context)
  {
    auto rc = fi_ep_bind(m_endpoint, &m_eq->fid, 0);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed binding ofi event queue to ofi endpoint, reason: ", fi_strerror(rc));

    rc = fi_ep_bind(m_endpoint, &m_rx_cq->fid, FI_RECV);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed binding ofi RX completion queue to ofi endpoint, reason: ", fi_strerror(rc));

    rc = fi_ep_bind(m_endpoint, &m_tx_cq->fid, FI_TRANSMIT);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed binding ofi TX completion queue to ofi endpoint, reason: ", fi_strerror(rc));
  }

  /// ctor #2
  explicit endpoint(boost::asio::io_context& io_context, const domain& domain)
  : endpoint(io_context, domain, domain.get_info())
  {
  }

  /// (default) ctor
  endpoint() = delete;

  /// copy ctor
  endpoint(const endpoint& rh) = delete;

  /// move ctor
  endpoint(endpoint&& rhs)
  : m_context(std::move(rhs.m_context))
  , m_io_context(rhs.m_io_context)
  , m_domain(std::move(rhs.m_domain))
  , m_endpoint(std::move(rhs.m_endpoint))
  , m_eq(rhs.m_eq)
  , m_eq_fd(std::move(rhs.m_eq_fd))
  , m_rx_cq(std::move(rhs.m_rx_cq))
  , m_tx_cq(std::move(rhs.m_tx_cq))
  , m_rx_cq_fd(std::move(rhs.m_rx_cq_fd))
  , m_tx_cq_fd(std::move(rhs.m_tx_cq_fd))
  , m_rx_strand(std::move(rhs.m_rx_strand))
  , m_tx_strand(std::move(rhs.m_tx_strand))
  {
    rhs.m_endpoint = nullptr;
    rhs.m_eq = nullptr;
    rhs.m_rx_cq = nullptr;
    rhs.m_tx_cq = nullptr;
  }

  /// transition endpoint to enabled state
  auto enable() -> void
  {
    auto rc = fi_enable(m_endpoint);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed transitioning ofi endpoint to enabled state, reason: ", fi_strerror(rc));
  }

  template<typename CompletionHandler>
  auto connect(CompletionHandler&& handler) -> void {
    auto rc = fi_connect(m_endpoint,
                         get_wrapped_obj(m_domain.get_fabric().get_info())->dest_addr,
                         nullptr,
                         0);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed initiating connection to ", "", " on ofi endpoint, reason: ", fi_strerror(rc));

    auto wait_set = &m_eq->fid;
    rc = fi_trywait(get_wrapped_obj(m_domain.get_fabric()), &wait_set, 1);
    if (rc == FI_SUCCESS) {
      m_eq_fd.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [&](const boost::system::error_code& error) {
          if (!error) {
            // fi_eq_cm_entry {
            // fid_t            fid;        [> fid associated with request <]
            // struct fi_info  *info;       [> endpoint information <]
            // uint8_t          data[];     [> app connection data <]
            // };
            fi_eq_cm_entry entry;
            uint32_t event;
            auto rc = fi_eq_sread(m_eq, &event, &entry, sizeof(entry), 100, 0);
            if (rc == -FI_EAVAIL) {
              // not implemented yet, see ft_eq_readerr()
              throw runtime_error("Error pending on event queue, handling not yet implemented.");
            } else if (rc < 0) {
              throw runtime_error("Failed reading from event queue, reason: ", fi_strerror(rc));
            } else {
              if (event == FI_CONNECTED) {
                handler();
              } else {
                throw runtime_error("Unexpected event in event queue, found: ", event);
              }
            }
          } else {
            // not implemented yet
          }
        }
      );
    } else {
      throw runtime_error("connect: Not yet implemented");
    }
  }

  template<typename CompletionHandler>
  auto accept(CompletionHandler&& handler) -> void
  {
    auto rc = fi_accept(m_endpoint, nullptr, 0);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed accepting connection, reason: ", fi_strerror(rc));

    auto wait_set = &m_eq->fid;
    rc = fi_trywait(get_wrapped_obj(m_domain.get_fabric()), &wait_set, 1);
    if (rc == FI_SUCCESS) {
      m_eq_fd.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [&](const boost::system::error_code& error) {
          if (!error) {
            // fi_eq_cm_entry {
            // fid_t            fid;        [> fid associated with request <]
            // struct fi_info  *info;       [> endpoint information <]
            // uint8_t          data[];     [> app connection data <]
            // };
            fi_eq_cm_entry entry;
            uint32_t event;
            auto rc = fi_eq_sread(m_eq, &event, &entry, sizeof(entry), 100, 0);
            if (rc == -FI_EAVAIL) {
              // not implemented yet, see ft_eq_readerr()
              throw runtime_error("Error pending on event queue, handling not yet implemented.");
            } else if (rc < 0) {
              throw runtime_error("Failed reading from event queue, reason: ", fi_strerror(rc));
            } else {
              if (event == FI_CONNECTED) {
                handler();
              } else {
                throw runtime_error("Unexpected event in event queue, found: ", event);
              }
            }
          } else {
            // not implemented yet
          }
        }
      );
    } else {
      throw runtime_error("accept: Not yet implemented");
    }
  }

  template<typename CompletionHandler>
  auto send(boost::asio::mutable_buffer buffer, CompletionHandler&& handler) -> void
  {
    fi_addr_t dummy_addr;
    memory_region mr(m_domain, buffer, mr::access::send);
    auto rc = fi_send(m_endpoint, buffer.data(), buffer.size(), nullptr, dummy_addr, &m_context);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed posting a TX buffer on ofi endpoint, reason: ", fi_strerror(rc));
    std::cout << "send buffer POSTED" << std::endl;
    auto wait_set = &m_tx_cq->fid;
    rc = fi_trywait(get_wrapped_obj(m_domain.get_fabric()), &wait_set, 1);
    if (rc == FI_SUCCESS) {
      m_tx_cq_fd.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
          boost::asio::bind_executor(m_tx_strand,
          [&, keep_mr_alive(std::move(mr))](const boost::system::error_code& error) {
            if (!error) {
              // struct fi_cq_data_entry {
              // void     *op_context; [> operation context <]
              // uint64_t flags;       [> completion flags <]
              // size_t   len;         [> size of received data <]
              // void     *buf;        [> receive data buffer <]
              // uint64_t data;        [> completion data <]
              // };
              fi_cq_data_entry entry;
              auto rc = fi_cq_sread(m_tx_cq, &entry, 1, nullptr, 100);
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
                rc = fi_cq_readerr(m_tx_cq, &error, 0);
                if (rc == -FI_EAGAIN) {
                  // should not happen
                } else if (rc < 0) {
                  throw runtime_error("Failed reading error entry from TX completion queue, reason: ", fi_strerror(rc));
                } else {
                  throw runtime_error("Failed TX completion, reason: ", fi_cq_strerror(m_tx_cq, error.prov_errno, error.err_data, nullptr, 0));
                }
              } else if (rc < 0) {
                throw runtime_error("Failed reading from TX completion queue, reason: ", fi_strerror(rc));
              } else {
                std::cout << "send buffer SENT" << std::endl;
                handler(boost::asio::mutable_buffer(entry.buf, entry.len));
              }
            } else {
              // not implemented yet
            }
          }
        )
      );
    } else {
      // struct fi_cq_data_entry {
      // void     *op_context; [> operation context <]
      // uint64_t flags;       [> completion flags <]
      // size_t   len;         [> size of received data <]
      // void     *buf;        [> receive data buffer <]
      // uint64_t data;        [> completion data <]
      // };
      fi_cq_data_entry entry;
      auto rc = fi_cq_sread(m_tx_cq, &entry, 1, nullptr, 100);
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
        rc = fi_cq_readerr(m_tx_cq, &error, 0);
        if (rc == -FI_EAGAIN) {
          // should not happen
        } else if (rc < 0) {
          throw runtime_error("Failed reading error entry from TX completion queue, reason: ", fi_strerror(rc));
        } else {
          throw runtime_error("Failed TX completion, reason: ", fi_cq_strerror(m_tx_cq, error.prov_errno, error.err_data, nullptr, 0));
        }
      } else if (rc < 0) {
        throw runtime_error("Failed reading completion entry from TX completion queue, reason: ", fi_strerror(rc));
      } else {
        handler(boost::asio::mutable_buffer(entry.buf, entry.len));
      }
    }
  }

  template<typename CompletionHandler>
  auto recv(boost::asio::mutable_buffer buffer, CompletionHandler&& handler) -> void
  {
    fi_addr_t dummy_addr;
    memory_region mr(m_domain, buffer, mr::access::recv);
    auto rc = fi_recv(m_endpoint, buffer.data(), buffer.size(), nullptr, dummy_addr, &m_context);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed posting a RX buffer on ofi endpoint, reason: ", fi_strerror(rc));

    auto wait_set = &m_rx_cq->fid;
    rc = fi_trywait(get_wrapped_obj(m_domain.get_fabric()), &wait_set, 1);
    if (rc == FI_SUCCESS) {
      m_rx_cq_fd.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
          boost::asio::bind_executor(m_rx_strand,
          [&, keep_mr_alive(std::move(mr))](const boost::system::error_code& error) {
            if (!error) {
              // struct fi_cq_data_entry {
              // void     *op_context; [> operation context <]
              // uint64_t flags;       [> completion flags <]
              // size_t   len;         [> size of received data <]
              // void     *buf;        [> receive data buffer <]
              // uint64_t data;        [> completion data <]
              // };
              fi_cq_data_entry entry;
              auto rc = fi_cq_sread(m_rx_cq, &entry, 1, nullptr, 100);
              if (rc == -FI_EAVAIL) {
                // not implemented yet, see ft_eq_readerr()
                throw runtime_error("Error pending on RX completion queue, handling not yet implemented.");
              } else if (rc < 0) {
                throw runtime_error("Failed reading from RX completion queue, reason: ", fi_strerror(rc));
              } else {
                handler(boost::asio::mutable_buffer(entry.buf, entry.len));
              }
            } else {
              // not implemented yet
            }
          }
        )
      );
    } else {
      throw runtime_error("recv: Not yet implemented");
    }
  }

  auto shutdown() -> void
  {
    auto rc = fi_shutdown(m_endpoint, 0);
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed shutting down ofi endpoint, reason: ", fi_strerror(rc));
  }

  /// dtor
  ~endpoint()
  {
    fi_close(&m_endpoint->fid);
    fi_close(&m_tx_cq->fid);
    fi_close(&m_rx_cq->fid);
    fi_close(&m_eq->fid);
  }

  private:
  fi_context m_context;
  boost::asio::io_context& m_io_context;
  const domain& m_domain;
  fid_ep* m_endpoint;
  fid_eq* m_eq;
  boost::asio::posix::stream_descriptor m_eq_fd;
  fid_cq* m_rx_cq;
  fid_cq* m_tx_cq;
  boost::asio::posix::stream_descriptor m_rx_cq_fd;
  boost::asio::posix::stream_descriptor m_tx_cq_fd;
  boost::asio::io_context::strand m_rx_strand;
  boost::asio::io_context::strand m_tx_strand;

  static auto create_endpoint(const domain& domain, const info& info, fi_context& context) -> fid_ep* 
  {
    fid_ep* ep;
    auto rc = fi_endpoint(get_wrapped_obj(domain),
                          get_wrapped_obj(info),
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

  static auto get_native_wait_fd(fid* obj) -> int
  {
    int fd;
    auto rc = fi_control(obj, FI_GETWAIT, static_cast<void*>(&fd));
    if (rc != FI_SUCCESS)
      throw runtime_error("Failed retrieving native wait fd from ofi event queue, reason: ", fi_strerror(rc));

    return fd;
  }

  enum class direction { rx, tx };

  static auto create_completion_queue(direction dir, const info& info, const domain& domain, fi_context& context) -> fid_cq*
  {
    fid_cq* cq;
    fi_cq_attr cq_attr = {
      0,                 // size_t               size;      [> # entries for CQ <]
      0,                 // uint64_t             flags;     [> operation flags <]
      FI_CQ_FORMAT_DATA, // enum fi_cq_format    format;    [> completion format <]
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

    return cq;
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
  template<typename CompletionHandler>
  auto listen(CompletionHandler&& handler) -> void
  {
    auto rc = fi_listen(m_pep);
    if (rc != 0)
      throw runtime_error("Failed listening on ofi passive_ep, reason: ", fi_strerror(rc));

    auto wait_set = &m_eq->fid;
    rc = fi_trywait(get_wrapped_obj(m_fabric), &wait_set, 1);
    if (rc == FI_SUCCESS) {
      m_eq_fd.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [&](const boost::system::error_code& error) {
          if (!error) {
            fi_eq_cm_entry entry;
            uint32_t event;
            auto rc = fi_eq_sread(m_eq, &event, &entry, sizeof(entry), 100, 0);
            if (rc == -FI_EAVAIL) {
              // not implemented yet, see ft_eq_readerr()
              throw runtime_error("Error pending on event queue, handling not yet implemented.");
            } else if (rc < 0) {
              throw runtime_error("Failed reading from event queue, reason: ", fi_strerror(rc));
            } else {
              if (event == FI_CONNREQ) {
                asiofi::info info(entry.info);
                handler(entry.fid, std::move(info));
              } else {
                throw runtime_error("Unexpected event in event queue, found: ", event);
              }
            }
          } else {
            // not implemented yet
          }
        }
      );
    } else {
      throw runtime_error("listen: Not yet implemented");
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
