/********************************************************************************
 *    Copyright (C) 2019 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_MEMORY_REGION
#define ASIOFI_MEMORY_REGION

#include <asiofi/errno.hpp>
#include <asiofi/domain.hpp>
#include <functional>
#include <type_traits>
#include <utility>

namespace asiofi {
  /**
   * @struct memory_region memory_region.hpp <include/asiofi/memory_region.hpp>
   * @brief A memory buffer registered for ofi access
   */
  struct memory_region
  {
    enum class access : uint64_t
    {
      send = FI_SEND,
      recv = FI_RECV,
      read = FI_READ,
      write = FI_WRITE,
      remote_read = FI_REMOTE_READ,
      remote_write = FI_REMOTE_WRITE
    };

    friend memory_region::access operator|(memory_region::access lhs,
                                           memory_region::access rhs)
    {
      using T = std::underlying_type<memory_region::access>::type;
      return static_cast<memory_region::access>(static_cast<T>(lhs)
                                                | static_cast<T>(rhs));
    }

    explicit memory_region(const domain& domain,
                           boost::asio::mutable_buffer buffer,
                           access access)
      : m_memory_region(
          std::move(create_memory_region(domain, buffer, access, m_context.get())))
    {
      // std::cout << "registered memory region: local_desc=" << desc() << " buf=" <<
      // buffer.data() << " size=" << buffer.size() << " access=0x" << std::hex <<
      // static_cast<uint64_t>(access) << std::dec << std::endl;
    }

    memory_region(const memory_region&) = default;

    memory_region(memory_region&&) = default;

    auto desc() -> void* { return fi_mr_desc(m_memory_region.get()); }

    // ~memory_region()
    // {
    // if (m_memory_region.get() && (m_memory_region.use_count() == 1))
    // std::cout << "unregistering memory region: local_desc=" << desc() << " fid_mr*=" <<
    // m_memory_region.get() << std::endl;
    // }

  private:
    using fid_mr_deleter = std::function<void(fid_mr*)>;

    std::shared_ptr<fi_context> m_context;
    std::shared_ptr<fid_mr> m_memory_region;

    static auto create_memory_region(const domain& domain,
                                     boost::asio::mutable_buffer buffer,
                                     access access,
                                     fi_context* context)
      -> std::unique_ptr<fid_mr, fid_mr_deleter>
    {
      fid_mr* mr;
      auto rc = fi_mr_reg(get_wrapped_obj(domain),
                          buffer.data(),
                          buffer.size(),
                          static_cast<uint64_t>(access),
                          0,
                          0,
                          0,
                          &mr,
                          context);
      if (rc != 0)
        throw runtime_error("Failed registering ofi memory region (",
                            buffer.data(),
                            ",",
                            buffer.size(),
                            "), reason: ",
                            fi_strerror(rc));

      return {mr, [](fid_mr* mr) {
                fi_close(&mr->fid);
                // std::cout << "fi_close: fid_mr*=" << mr << std::endl;
              }};
    }
  }; /* struct memory_region */

  using mr = memory_region;

}   // namespace asiofi

#endif /* ifndef ASIOFI_MEMORY_REGION */
