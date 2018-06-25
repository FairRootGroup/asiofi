/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <asiofi.hpp>
#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/pmr/unsynchronized_pool_resource.hpp>
#include <boost/program_options.hpp>
#include <benchmark/benchmark.h>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <sys/mman.h>

namespace bpo = boost::program_options;

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

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
    ("port,p", bpo::value<std::string>()->default_value("5000"), "Server port")
    ("server,s", "Run server, otherwise client")
    ("provider,P", bpo::value<std::string>()->default_value("sockets"), "Provider")
    ("message-size,m", bpo::value<size_t>()->default_value(1024*1024), "Message size in Byte")
    ("iterations,i", bpo::value<size_t>()->default_value(100), "Number of messages to transfer")
    ("queue-size,q", bpo::value<size_t>()->default_value(10), "Maximum number of transfers to queue in parallel");

  
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
    std::cout << "  afi_msg_bw [OPTIONS] -s <ip>      Start server" << std::endl;
    std::cout << "  afi_msg_bw [OPTIONS] <ip>         Connect to server" << std::endl;
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

auto client(const std::string& address,
            const std::string& port,
            const std::string& provider,
            size_t message_size,
            size_t iterations,
            size_t queue_size) -> int
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

  asiofi::hints hints;
  hints.set_provider(provider);
  asiofi::info info(address.c_str(), port.c_str(), 0, hints);
  // std::cout << info << std::endl;
  asiofi::fabric fabric(info);
  asiofi::domain domain(fabric);
  asiofi::endpoint endpoint(io_context, domain);
  endpoint.enable();

  asiofi::allocated_pool_resource pool_mr;
  size_t received(0);
  size_t posted(0);
  std::atomic<size_t> queued(0);
  std::chrono::time_point<std::chrono::steady_clock> start;
  std::chrono::time_point<std::chrono::steady_clock> stop;
  boost::asio::steady_timer timer(io_context);

  std::function<void(const boost::system::error_code&)> post_recv_buffer;

  auto recv_handler = [&](boost::asio::mutable_buffer buffer) {
    assert(buffer.size() == message_size);
    pool_mr.deallocate(buffer.data(), buffer.size());
    ++received;
    --queued;
    if (received == iterations) {
      stop = std::chrono::steady_clock::now();
      endpoint.shutdown();
      signals.cancel();
      auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
      auto rate_MiB = (iterations * message_size * 1000.) / (1024. * 1024. * elapsed_ms);
      auto rate_MB = (iterations * message_size * 1000.) / (1000. * 1000. * elapsed_ms);
      std::cout << "elapsed time: " << elapsed_ms << " ms  data sent: " <<  (iterations * message_size) << " Bytes" << std::endl; 
      std::cout << "bandwidth used: " << rate_MiB << " MiB/s  " << rate_MB << " MB/s" << std::endl;
    }
  };

  post_recv_buffer = [&](const boost::system::error_code& error) {
    while (queued < queue_size && posted < iterations) {
			boost::asio::mutable_buffer buffer(pool_mr.allocate(message_size), message_size);
			endpoint.recv(buffer, recv_handler);
			++queued;
      ++posted;
    }

    if (posted < iterations) {
			timer.expires_after(std::chrono::milliseconds(50));
			timer.async_wait(post_recv_buffer);
    }
  };

  auto connect_handler = [&]{
    start = std::chrono::steady_clock::now();
    post_recv_buffer(boost::system::error_code());
  };

  endpoint.connect(connect_handler);
  io_context.run();

  return EXIT_SUCCESS;
}

auto server(const std::string& address,
            const std::string& port,
            const std::string& provider,
            size_t message_size,
            size_t iterations,
            size_t queue_size) -> int
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

  asiofi::hints hints;
  hints.set_provider(provider);
  asiofi::info info(address.c_str(), port.c_str(), FI_SOURCE, hints);
  // std::cout << info << std::endl;
  asiofi::fabric fabric(info);
  asiofi::domain domain(fabric);
  asiofi::passive_endpoint pep(io_context, fabric);
  std::unique_ptr<asiofi::endpoint> endpoint(nullptr);

  asiofi::allocated_pool_resource pool_mr;
  size_t sent(0);
  size_t posted(0);
  std::atomic<size_t> queued(0);
  std::chrono::time_point<std::chrono::steady_clock> start;
  std::chrono::time_point<std::chrono::steady_clock> stop;
  boost::asio::steady_timer timer(io_context);

  std::function<void(const boost::system::error_code&)> post_send_buffer;

  auto send_handler = [&](boost::asio::mutable_buffer buffer) {
    if (sent == 0) {
      start = std::chrono::steady_clock::now();
    }
    assert(buffer.size() == message_size);
    pool_mr.deallocate(buffer.data(), buffer.size());
    ++sent;
    --queued;
    if (sent == iterations) {
      stop = std::chrono::steady_clock::now();
      endpoint->shutdown();
      signals.cancel();
      auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
      auto rate_MiB = (iterations * message_size * 1000.) / (1024. * 1024. * elapsed_ms);
      auto rate_MB = (iterations * message_size * 1000.) / (1000. * 1000. * elapsed_ms);
      std::cout << "elapsed time: " << elapsed_ms << " ms  data sent: " <<  (iterations * message_size) << " Bytes" << std::endl; 
      std::cout << "bandwidth used: " << rate_MiB << " MiB/s  " << rate_MB << " MB/s" << std::endl;
    }
  };

  post_send_buffer = [&](const boost::system::error_code& error) {
    while (queued < queue_size && posted < iterations) {
      boost::asio::mutable_buffer buffer(pool_mr.allocate(message_size), message_size);
      endpoint->send(buffer, send_handler);
			++queued;
      ++posted;
    } 

    if (posted < iterations) {
      timer.expires_after(std::chrono::milliseconds(50));
      timer.async_wait(post_send_buffer);
    }
  };

  auto accept_handler = [&]() {
    post_send_buffer(boost::system::error_code());
  };

  auto listen_handler = [&](fid_t handle, asiofi::info info) {
    endpoint = make_unique<asiofi::endpoint>(io_context, domain, info);
    endpoint->enable();
    endpoint->accept(accept_handler);
  };

  pep.listen(listen_handler);
  io_context.run();

  return EXIT_SUCCESS;
}

auto main(int argc, char** argv) -> int
{
  bpo::variables_map vm;
  handle_cli(argc, argv, vm);

  if (vm.count("server")) {
    return server(vm["host"].as<std::string>(),
                  vm["port"].as<std::string>(),
                  vm["provider"].as<std::string>(),
                  vm["message-size"].as<size_t>(),
                  vm["iterations"].as<size_t>(),
                  vm["queue-size"].as<size_t>());
  } else {
    return client(vm["host"].as<std::string>(),
                  vm["port"].as<std::string>(),
                  vm["provider"].as<std::string>(),
                  vm["message-size"].as<size_t>(),
                  vm["iterations"].as<size_t>(),
                  vm["queue-size"].as<size_t>());
  }
}
