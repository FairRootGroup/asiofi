/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_MEMORY_RESOURCES_HPP
#define ASIOFI_MEMORY_RESOURCES_HPP

#include <asiofi/domain.hpp>
#include <atomic>
#include <boost/container/pmr/synchronized_pool_resource.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/pmr/global_resource.hpp>
#include <boost/container/pmr/pool_options.hpp>
#include <boost/container/pmr/memory_resource.hpp>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <tuple>
#include <utility>

namespace asiofi
{

/**
 * @struct registered_memory_resource memory_resources.hpp <asiofi/memory_resources.hpp>
 * @brief Works just like the provided upstream allocator, but adds initialization and OFI
 *        memory region registration.
 */
struct registered_memory_resource : boost::container::pmr::memory_resource
{
  registered_memory_resource(const asiofi::domain& domain,
      std::size_t size,
      boost::container::pmr::memory_resource* upstream = boost::container::pmr::get_default_resource())
  : m_ptr(upstream->allocate(size))
  , m_size(size)
  , m_region(create_region(domain))
  , m_upstream(m_ptr, m_size, upstream)
  {
    //std::cout << "m_ptr=" << m_ptr << "m_size=" << m_size << std::endl;
  }

  auto get_region() -> asiofi::memory_region&
  {
    return m_region;
  }

  protected:
  auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override
  {
    return m_upstream.allocate(bytes, alignment);
  }

  auto do_deallocate(void* p, std::size_t bytes, std::size_t alignment) -> void override
  {
    return m_upstream.deallocate(p, bytes, alignment);
  }

  auto do_is_equal(const memory_resource& other) const noexcept -> bool
  {
    return m_upstream.is_equal(other);
  }

  private:
  void* m_ptr;
  std::size_t m_size;
  asiofi::memory_region m_region;
  boost::container::pmr::monotonic_buffer_resource m_upstream;

  auto create_region(const asiofi::domain& domain) -> asiofi::memory_region
  {
    std::memset(m_ptr, 1, m_size);
    return asiofi::memory_region(domain,
            boost::asio::mutable_buffer(m_ptr, m_size),
            asiofi::mr::access::recv | asiofi::mr::access::send);
  }
}; /* struct allocated_pool_memory_resource */

} /* namespace asiofi */

#endif /* ASIOFI_MEMORY_RESOURCES_HPP */
