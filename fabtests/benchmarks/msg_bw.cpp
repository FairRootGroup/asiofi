/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <asiofi.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <benchmark/benchmark.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/mman.h>

namespace bpo = boost::program_options;

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

auto handle_cli(int argc, char** argv, bpo::variables_map& vm) -> void
try {
  bpo::options_description opts{"Options"};
  opts.add_options()
    ("help,h", "Help screen")
    ("version,v", "Print version")
    ("memory", "Run local memory benchmark")
    ("port,p", bpo::value<std::string>()->default_value("5000"), "Server port");
  
  bpo::options_description hidden;
  hidden.add_options()
    ("host", bpo::value<std::string>(), "Host to connect to");
  
  bpo::options_description all;
  all.add(opts).add(hidden);

  bpo::positional_options_description pos_opts;
  pos_opts.add("host", 1);

  bpo::store(bpo::command_line_parser(argc, argv).options(all).positional(pos_opts).run(), vm);
  bpo::notify(vm);

  if (vm.count("help")) {
    std::cout << "Usage:" << std::endl;
    std::cout << "  afi_msg_bw [OPTIONS]                Start server" << std::endl;
    std::cout << "  afi_msg_bw [OPTIONS] <host>         Connect to server" << std::endl;
    std::cout << std::endl << "Bandwidth test for MSG endpoints." << std::endl << std::endl;
    std::cout << opts << std::endl;
    std::exit(EXIT_SUCCESS);
  } else if (vm.count("version")) {
    std::cout << "asiofi " << ASIOFI_GIT_VERSION << std::endl;
    std::exit(EXIT_SUCCESS);
  } else if (vm.count("memory")) {
    benchmark::Initialize(&argc, argv);

    benchmark::RegisterBenchmark("naive", &bm_naive)->
      RangeMultiplier(2)->Range(1<<12, 1<<29)->Threads(1);
    benchmark::RegisterBenchmark("naive_hp", &bm_naive_hp)->
      RangeMultiplier(2)->Range(1<<21, 1<<29)->Threads(1);
    benchmark::RegisterBenchmark("reuse", &bm_reuse)->
      RangeMultiplier(2)->Range(1<<12, 1<<29)->Threads(1);
    benchmark::RegisterBenchmark("reuse_hp", &bm_reuse_hp)->
      RangeMultiplier(2)->Range(1<<21, 1<<29)->Threads(1);

    benchmark::RunSpecifiedBenchmarks();
    std::exit(EXIT_SUCCESS);
  }
}
catch (const bpo::error& ex)
{
  std::cerr << ex.what() << std::endl;
  std::exit(EXIT_FAILURE);
}

auto client(const std::string& address, const std::string& port) -> int
{
  boost::asio::io_context io_context;

  boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](const boost::system::error_code& error, int signal_number) {
    if (error && error != boost::asio::error::operation_aborted) {
      std::cerr << "Signal handler: Received error code " << error << std::endl;
    } else {
      io_context.stop();
    }
  });

  asiofi::info info(address.c_str(), port.c_str(), 0, asiofi::hints());
  // std::cout << info << std::endl;
  asiofi::fabric fabric(info);
  asiofi::domain domain(fabric);
  asiofi::endpoint ep(io_context, domain);
  ep.connect();

  io_context.run();

  return EXIT_SUCCESS;
}

auto server(const std::string& address, const std::string& port) -> int
{
  boost::asio::io_context io_context;
  
  boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](const boost::system::error_code& error, int signal_number) {
    if (error && error != boost::asio::error::operation_aborted) {
      std::cerr << "Signal handler: Received error code " << error << std::endl;
    } else {
      io_context.stop();
    }
  });

  asiofi::info info(address.c_str(), port.c_str(), FI_SOURCE, asiofi::hints());
  // std::cout << info << std::endl;
  asiofi::fabric fabric(info);
  asiofi::domain domain(fabric);
  asiofi::passive_endpoint pep(io_context, fabric);
  pep.listen();

  io_context.run();

  // fi_cq_attr cq_attr_rx = {0, 0, FI_CQ_FORMAT_DATA, FI_WAIT_UNSPEC, 0, FI_CQ_COND_NONE, nullptr};
  // cq_attr_rx.size = info->rx_attr->size;
  // fi_cq_attr cq_attr_tx = {0, 0, FI_CQ_FORMAT_DATA, FI_WAIT_UNSPEC, 0, FI_CQ_COND_NONE, nullptr};
  // cq_attr_tx.size = info->tx_attr->size;
  // size_t               size;      [> # entries for CQ <]
  // uint64_t             flags;     [> operation flags <]
  // enum fi_cq_format    format;    [> completion format <]
  // enum fi_wait_obj     wait_obj;  [> requested wait object <]
  // int                  signaling_vector; [> interrupt affinity <]
  // enum fi_cq_wait_cond wait_cond; [> wait condition format <]
  // struct fid_wait     *wait_set;  [> optional wait set <]
  // ret = fi_cq_open(fdomain, &attr_rx, &cq_rx, &ctx);
  // if (ret != FI_SUCCESS)
    // std::cerr << "Failed creating ofi RX completion queue, reason: " << fi_strerror(ret) << std::endl;
  // ret = fi_cq_open(fdomain, &attr_tx, &cq_tx, &ctx);
  // if (ret != FI_SUCCESS)
    // std::cerr << "Failed creating ofi TX completion queue, reason: " << fi_strerror(ret) << std::endl;
//
//
	// if (av) {
    // auto ret = fi_close(&av->fid);
    // if (ret != FI_SUCCESS)
      // std::cerr << "Failed closing ofi address vector, reason: " << fi_strerror(ret) << std::endl;
  // }

  return EXIT_SUCCESS;
}

auto main(int argc, char** argv) -> int
{
  bpo::variables_map vm;
  handle_cli(argc, argv, vm);

  if (vm.count("host")) {
    return client(vm["host"].as<std::string>(), vm["port"].as<std::string>());
  } else {
    return server("127.0.0.1", vm["port"].as<std::string>());
  }
}
