/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <benchmark/benchmark.h>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdlib>
#include <new>
#include <iostream>

auto bm_naive(benchmark::State& state) -> void
{
  const size_t size = state.range(0);
  const auto step = sizeof(size_t);

  for (auto _ : state) {
    const auto x = static_cast<size_t*>(std::malloc(size));
    const auto end = x + size / sizeof(size_t);
    for (auto it = x; it < end; it += step / sizeof(size_t)) *it = size_t();
    std::free(x);
  }

  state.SetBytesProcessed(state.iterations() * size);
}

auto bm_naive_hp(benchmark::State& state) -> void
{
  const size_t size = state.range(0);
  const auto step = sizeof(size_t);

  for (auto _ : state) {
    const auto x = static_cast<size_t*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0));
    if (x != MAP_FAILED) {
      const auto end = x + size / sizeof(size_t);
      for (auto it = x; it < end; it += step / sizeof(size_t)) *it = size_t();
      munmap(static_cast<void*>(x), size);
    } else {
      std::cerr << "Allocation failed." << std::endl;
      std::exit(EXIT_FAILURE);
    }
  }

  state.SetBytesProcessed(state.iterations() * size);
}

auto bm_reuse(benchmark::State& state) -> void
{
  const size_t size = state.range(0);
  const auto step = sizeof(size_t);
  const auto x = static_cast<size_t*>(std::malloc(size));
  const auto end = x + size / sizeof(size_t);
  for (auto it = x; it < end; it += step / sizeof(size_t)) *it = size_t();

  for (auto _ : state) {
    for (auto it = x; it < end; it += step / sizeof(size_t)) *it = size_t();
  }

  std::free(x);
  state.SetBytesProcessed(state.iterations() * size);
}

auto bm_reuse_hp(benchmark::State& state) -> void
{
  const size_t size = state.range(0);
  const auto step = sizeof(size_t);
  const auto x = static_cast<size_t*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0));
  if (x != MAP_FAILED) {
    const auto end = x + size / sizeof(size_t);
    for (auto it = x; it < end; it += step / sizeof(size_t)) *it = size_t();

    for (auto _ : state) {
      for (auto it = x; it < end; it += step / sizeof(size_t)) *it = size_t();
    }

    munmap(static_cast<void*>(x), size);
    state.SetBytesProcessed(state.iterations() * size);
  } else {
    std::cerr << "Allocation failed." << std::endl;
    std::exit(EXIT_FAILURE);
  }
}

int main(int argc, char** argv)
{
  using namespace boost::interprocess;

  //Remove shared memory on construction and destruction
  struct shm_remove {
    shm_remove() { shared_memory_object::remove("shmalloc_bw"); }
    ~shm_remove() { shared_memory_object::remove("shmalloc_bw"); }
  } remover;

  managed_shared_memory segment(create_only, "shmalloc_bw", 1<<30);

  auto bm = [&](benchmark::State& state){
    const size_t size = state.range(0);

    for (auto _ : state) {
      const auto x = static_cast<size_t*>(std::malloc(size));
      benchmark::DoNotOptimize(x);
      std::free(x);
    }

    state.SetBytesProcessed(state.iterations() * size);
  };

  auto bm2 = [&](benchmark::State& state){
    const size_t size = state.range(0);

    for (auto _ : state) {
      const auto x = static_cast<size_t*>(new size_t[size / sizeof(size_t)]);
      benchmark::DoNotOptimize(x);
      delete[] x;
    }

    state.SetBytesProcessed(state.iterations() * size);
  };

  auto bm3 = [&](benchmark::State& state){
    const size_t size = state.range(0);

    for (auto _ : state) {
      const auto x = static_cast<size_t*>(segment.allocate(size));
      benchmark::DoNotOptimize(x);
      segment.deallocate(x);
    }

    state.SetBytesProcessed(state.iterations() * size);
  };
  
  benchmark::RegisterBenchmark("malloc, free", bm)->
    RangeMultiplier(2)->Range(1<<12, 1<<29)->Threads(1);
  benchmark::RegisterBenchmark("new, delete", bm2)->
    RangeMultiplier(2)->Range(1<<12, 1<<29)->Threads(1);
  benchmark::RegisterBenchmark("shmalloc, shmdealloc", bm3)->
    RangeMultiplier(2)->Range(1<<12, 1<<29)->Threads(1);

  benchmark::RegisterBenchmark("naive", &bm_naive)->
    RangeMultiplier(2)->Range(1<<12, 1<<29)->Threads(1);
  // benchmark::RegisterBenchmark("naive_hp", &bm_naive_hp)->
    // RangeMultiplier(2)->Range(1<<21, 1<<29)->Threads(1);
  benchmark::RegisterBenchmark("reuse", &bm_reuse)->
    RangeMultiplier(2)->Range(1<<12, 1<<29)->Threads(1);
  // benchmark::RegisterBenchmark("reuse_hp", &bm_reuse_hp)->
    // RangeMultiplier(2)->Range(1<<21, 1<<29)->Threads(1);

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();

  return 0;
}
