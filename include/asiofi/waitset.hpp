/********************************************************************************
 *    Copyright (C) 2019 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_WAITSET
#define ASIOFI_WAITSET

#include <asiofi/errno.hpp>
#include <asiofi/domain.hpp>

namespace asiofi {
  /**
   * @struct waitset waitset.hpp <include/asiofi/waitset.hpp>
   * @brief A waitset enables an optimized method of waiting for events across multiple
   * queues.
   */
  struct waitset
  {
    /// get wrapped C object
    friend auto get_wrapped_obj(const waitset& waitset) -> fid_wait*
    {
      return waitset.m_waitset.get();
    }

    explicit waitset(const domain& domain)
      : m_waitset(std::move(create_waitset(domain)))
    {
    }

    waitset(const waitset&) = delete;

    waitset(waitset&&) = default;

    auto wait(int timeout = 0) -> void
    {
      auto rc = fi_wait(m_waitset.get(), timeout);
      if (rc != FI_SUCCESS)
        throw runtime_error(rc, "Failed waiting on ofi waitset");
    }
  private:
    using fid_wait_deleter = std::function<void(fid_wait*)>;

    std::shared_ptr<fid_wait> m_waitset;

    static auto create_waitset(const domain& domain)
      -> std::unique_ptr<fid_wait, fid_wait_deleter>
    {
      fid_wait* ws;
      fi_wait_attr attr{
        FI_WAIT_UNSPEC,   // enum fi_wait_obj wait_obj; /* requested wait object */
        0                 // uint64_t         flags;    /* operation flags *
      };
      auto rc = fi_wait_open(get_wrapped_obj(domain.get_fabric()), &attr, &ws);
      if (rc != FI_SUCCESS)
        throw runtime_error(rc, "Failed creating ofi waitset");

      return {ws, [](fid_wait* ws2) { fi_close(&ws2->fid); }};
    }
  }; /* struct memory_region */

}   // namespace asiofi

#endif /* ifndef ASIOFI_WAITSET */
