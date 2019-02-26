/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_ADDRESS_VECTOR_HPP
#define ASIOFI_ADDRESS_VECTOR_HPP

#include <asiofi/errno.hpp>
#include <asiofi/domain.hpp>

namespace asiofi
{
  /**
   * @struct address_vector address_vector.hpp <asiofi/address_vector.hpp>
   * @brief Wraps fid_av
   */
  struct address_vector
  {
    /// ctor #1
    explicit address_vector(const domain& domain)
    : m_context({nullptr, nullptr, nullptr, nullptr})
    {
      // TODO factor out into static member functions
      fi_av_attr av_attr = {
        get_wrapped_obj(domain.get_info())->domain_attr->av_type,
                 // enum fi_av_type  type;        [> type of AV <]
        0,       // int              rx_ctx_bits; [> address bits to identify rx ctx <]
        1000,    // size_t           count;       [> # entries for AV <]
        0,       // size_t           ep_per_node; [> # endpoints per fabric address <]
        nullptr, // const char       *name;       [> system name of AV <]
        nullptr, // void             *map_addr;   [> base mmap address <]
        0        // uint64_t         flags;       [> operation flags <]
      };
      auto rc = fi_av_open(get_wrapped_obj(domain), &av_attr, &m_av, &m_context);
      if (rc != 0)
        throw runtime_error(rc, "Failed opening ofi address vector");
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
    fid_av* m_av; // TODO use smart pointer
  }; /* struct address_vector */

  using av = address_vector;

} /* namespace asiofi */

#endif /* ASIOFI_ADDRESS_VECTOR_HPP */
