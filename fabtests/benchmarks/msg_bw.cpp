/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <asiofi/fabric.hpp>
#include <asiofi/version.hpp>
#include <benchmark/benchmark.h>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv)
{
  std::cout << "asiofi version " << ASIOFI_GIT_VERSION << " from " << ASIOFI_GIT_DATE << std::endl;

  asiofi::hints hints;
  asiofi::info info(0, hints);
  std::cout << info << std::endl;

  auto bm = [&](benchmark::State& state){
    const size_t size = state.range(0);
    const auto step = sizeof(size_t);

    for (auto _ : state) {
      const auto x = static_cast<size_t*>(std::malloc(size));
      const auto end = x + size / sizeof(size_t);
      for (auto it = x; it < end; it += step / sizeof(size_t)) *it = size_t();
      std::free(x);
    }

    state.SetBytesProcessed(state.iterations() * size);
  };

  auto bm2 = [&](benchmark::State& state){
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
  };

  benchmark::RegisterBenchmark("naive", bm)->
    RangeMultiplier(2)->Range(1<<12, 1<<29)->Threads(1);
  benchmark::RegisterBenchmark("reuse", bm2)->
    RangeMultiplier(2)->Range(1<<12, 1<<29)->Threads(1);

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();

  return 0;
}
