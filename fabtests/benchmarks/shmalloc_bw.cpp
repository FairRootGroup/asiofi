/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <benchmark/benchmark.h>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <unistd.h>

int main(int argc, char** argv)
{
  using namespace boost::interprocess;

  //Remove shared memory on construction and destruction
  struct shm_remove {
    shm_remove() { shared_memory_object::remove("shmalloc_bw"); }
    ~shm_remove() { shared_memory_object::remove("shmalloc_bw"); }
  } remover;

  managed_shared_memory segment(create_only, "shmalloc_bw", 1<<30);

  const size_t page = sysconf(_SC_PAGESIZE);

  auto bm = [&](benchmark::State& state){
    const size_t msg = state.range(0);
    const size_t step = msg / page;
    const size_t step2 = step / sizeof(int);

    for (auto _ : state) {
      auto x = static_cast<int*>(segment.allocate(msg));
      benchmark::DoNotOptimize(x);
      auto x2 = x;
      for (size_t i = 0; i < msg; i += step) {
        *x2 = 42;
        benchmark::DoNotOptimize(x2);
        x2 += step2;
      }
      segment.deallocate(x);
    }
    state.SetBytesProcessed(state.iterations() * msg);
  };
  
  benchmark::RegisterBenchmark("shmalloc, page write, shmdealloc", bm)->
    RangeMultiplier(2)->Range(1<<12, 1<<29)->Threads(1);

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();

  return 0;
}
