/********************************************************************************
 * Copyright (C) 2018-2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_MEMORY_RESOURCES_HPP
#define ASIOFI_MEMORY_RESOURCES_HPP

#include <asiofi/memory_region.hpp>
#include <atomic>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory_resource>
#include <sys/mman.h>
#include <unordered_set>
#include <utility>
#include <vector>

namespace asiofi
{

/**
 * @struct allocated_pool_resource allocators.hpp <asiofi/memory_resources.hpp>
 * @brief Works just like std::pmr::synchronized_pool_resource, but physically
 *        allocates new buffers.
 */
struct allocated_pool_resource : std::pmr::synchronized_pool_resource
{
  allocated_pool_resource()
  : synchronized_pool_resource()
  , m_hit(0)
  , m_total(0)
  {
  }

  ~allocated_pool_resource() override
  {
    double fast_alloc_rate = (m_hit * 100.) / (m_total * 1.);
    std::cout << std::fixed << std::setprecision(2) << fast_alloc_rate << "% reused allocations of " << m_total << " total allocations" << std::endl;
  }

  protected:
  auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override
  {
    auto ptr = std::pmr::synchronized_pool_resource::do_allocate(bytes, alignment);

    if (m_allocated.insert(ptr).second) {
      std::memset(ptr, 0, bytes); // TODO see if we need stronger page pinning here
      // std::cout << "allocated: ptr=" << ptr << ", size=" << bytes << std::endl;
    } else {
      // std::cout << "allocated (fast): ptr=" << ptr << ", size=" << bytes << std::endl;
      ++m_hit;
    }
    ++m_total;

    return ptr;
  }

  auto do_deallocate(void* p, std::size_t bytes, std::size_t alignment) -> void override
  {
    std::pmr::synchronized_pool_resource::do_deallocate(p, bytes, alignment);

    // std::cout << "deallocated: ptr=" << p << ", size=" << bytes << std::endl;
  }

  // auto do_is_equal(const memory_resource& other) const -> bool noexcept
  // {
//
  // }

  private:
  std::unordered_set<void*> m_allocated;
  std::atomic<size_t> m_hit, m_total;
}; /* struct allocated_pool_memory_resource */

/**
 * @struct registered_memory_resource memory_resources.hpp <asiofi/memory_resources.hpp>
 * @brief Works just like monotonic memory resource, but adds mlock and OFI
 *        memory region registration to the first upstream allocation.
 */
struct registered_memory_resource : std::pmr::memory_resource
{
  registered_memory_resource(const asiofi::domain& domain,
      std::size_t size,
      std::pmr::memory_resource* upstream = std::pmr::get_default_resource())
  : m_ptr(upstream->allocate(size))
  , m_size(size)
  , m_region(create_region(domain))
  , m_upstream(m_ptr, m_size, upstream)
  {
    //std::cout << "m_ptr=" << m_ptr << "m_size=" << m_size << std::endl;
  }

  ~registered_memory_resource()
  {
    munlock(m_ptr, m_size);
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

  auto do_is_equal(const memory_resource& other) const noexcept -> bool override
  {
    return m_upstream.is_equal(other);
  }

  private:
  void* m_ptr;
  std::size_t m_size;
  asiofi::memory_region m_region;
  std::pmr::monotonic_buffer_resource m_upstream;

  auto create_region(const asiofi::domain& domain) -> asiofi::memory_region
  {
    mlock(m_ptr, m_size);
    return asiofi::memory_region(domain,
            boost::asio::mutable_buffer(m_ptr, m_size),
            asiofi::mr::access::recv | asiofi::mr::access::send);
  }
}; /* struct registered_memory_resource */

} /* namespace asiofi */

#endif /* ASIOFI_MEMORY_RESOURCES_HPP */
