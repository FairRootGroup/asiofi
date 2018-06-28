/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_MEMORY_RESOURCES_HPP
#define ASIOFI_MEMORY_RESOURCES_HPP

#include <boost/container/pmr/unsynchronized_pool_resource.hpp>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>
#include <unordered_set>
#include <utility>

namespace asiofi
{

/**
 * @struct allocated_pool_resource allocators.hpp <asiofi/memory_resources.hpp>
 * @brief Works just like boost::container::pmr::unsynchronized_pool_resource, but physically
 *        allocates new buffers.
 */
struct allocated_pool_resource : boost::container::pmr::unsynchronized_pool_resource
{
  allocated_pool_resource()
  : unsynchronized_pool_resource()
  , m_total(0)
  , m_hit(0)
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
    auto ptr = boost::container::pmr::unsynchronized_pool_resource::do_allocate(bytes, alignment);

    if (m_allocated.insert(ptr).second) {
      std::memset(ptr, 0, bytes);
      // std::cout << "allocated: ptr=" << ptr << ", size=" << bytes << std::endl;
    } else {
      ++m_hit;
      // std::cout << "allocated (fast): ptr=" << ptr << ", size=" << bytes << std::endl;
    }

    ++m_total;
    return ptr;
  }

  // auto do_deallocate(void* p, std::size_t bytes, std::size_t alignment) -> void override
  // {
    // boost::container::pmr::unsynchronized_pool_resource::do_deallocate(p, bytes, alignment);
//
    // std::cout << "deallocated: ptr=" << p << ", size=" << bytes << std::endl;
  // }

  // auto do_is_equal(const memory_resource& other) const -> bool noexcept
  // {
//
  // }

  private:
  std::unordered_set<void*> m_allocated;
  size_t m_total;
  size_t m_hit;
}; /* struct allocated_pool_memory_resource */

} /* namespace asiofi */

#endif /* ASIOFI_MEMORY_RESOURCES_HPP */
