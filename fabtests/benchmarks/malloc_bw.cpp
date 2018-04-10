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

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();

  return 0;
}
